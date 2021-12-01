#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <editline/readline.h>
#include <editline/history.h>

#include "lisp.h"

#define die(fmt, ...)                                                          \
	{                                                                      \
		fprintf(stderr, fmt, __VA_ARGS__);                             \
		exit(EXIT_FAILURE);                                            \
	}                                                                      \
	while (0)

#define xmalloc(size)                                                          \
	({                                                                     \
		void *_ret = malloc(size);                                     \
		if (!_ret)                                                     \
			die("%s", "failed to allocate memory\n");              \
		_ret;                                                          \
	})

static char *version = "Lisp Version 0.0.0.0.1";

enum {
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_CHARBUF,
	LVAL_FUN,
	LVAL_SEXPR,
	LVAL_QEXPR,
};

enum {
	LERR_DIV_ZERO,
	LERR_BAD_OP,
	LERR_BAD_NUM,
};

/* current min size is int + 4 pointers (function lval)
 * which gives us a size of 4 + 8(4) = 36 Bytes on 64b
 * and 4 + 4(4) = 20 Bytest on 32b
 */
struct lval {
	int type;

	union {
		/* basic */
		union {
			char *sym;
			char *charbuf;
			char *err;
			long num;
		};

		/* Function */
		struct {
			lbuiltin builtin;
			struct lenv *env;
			struct lval *formals;
			struct lval *body;
		};

		/* Expression */
		struct {
			struct lval **cell;
			int count;
		};
	};
};

struct lenv {
	struct lenv *par;
	struct lval **vals;
	char **syms;
	int count;
};

/* lval constructors */
static struct lval *lval_num(long x)
{
	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

static struct lval *lval_call(struct lenv *e, struct lval *f, struct lval *a)
{
	if (f->builtin)
		return f->builtin(e, a);
	const char *fname = lenv_lookup_sym_by_val(e, f);

	while (a->count) {
		if (f->formals->count == 0) {
			lval_free(a);
			return lerr_args_too_few_variable(f->formals, fname, 1);
		}

		/* pop first symbol from formals list */
		struct lval *sym = lval_pop(f->formals, 0);

		/* add support for variable arguments via '&' symbol*/
		if (strcmp(sym->sym, "&") == 0) {
			if (f->formals->count != 1) {
				lval_free(a);
				return lval_func_err(
					f->formals, fname,
					"& must be followed by another symbol");
			}

			/* Bind variable formal to remaining args */
			struct lval *nsym = lval_pop(f->formals, 0);
			lenv_put(f->env, nsym, builtin_list(e, a));
			lval_free(sym);
			lval_free(nsym);
			break;
		}

		/* pop first arg from list */
		struct lval *val = lval_pop(a, 0);

		/* Bind a copy into the function's environment */
		lenv_put(f->env, sym, val);

		/* Delete symbol and value */
		lval_free(sym);
		lval_free(val);
	}

	/* arg list is bound, clean up */
	lval_free(a);

	if (f->formals->count == 0) {
		/* set environment parent to evaluation environment */
		f->env->par = e;

		/* evaluate and return */
		return builtin_eval(f->env,
				    lval_add(lval_sexpr(), lval_copy(f->body)));
	} else {
		/* return a partial */
		return lval_copy(f);
	}
}

static struct lval *lval_err(const char *fmt, ...)
{
	const int size = 512;
	va_list va;
	va_start(va, fmt);

	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_ERR;

	v->err = xmalloc(size);
	vsnprintf(v->err, size, fmt, va);
	va_end(va);
	return v;
}

static struct lval *lval_sym(char *s)
{
	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_SYM;
	v->sym = xmalloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

/* TODO: use strncpy() here instead */
static struct lval *lval_read_str(mpc_ast_t *t)
{
	t->contents[strlen(t->contents) - 1] = '\0';
	char *unescaped = malloc(strlen(t->contents + 1) + 1);
	strcpy(unescaped, t->contents + 1);

	/* pass through the unescape function */
	unescaped = mpcf_unescape(unescaped);
	struct lval *str = lval_str(unescaped);
	free(unescaped);
	return str;
}

static struct lval *lval_str(char *s)
{
	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_CHARBUF;
	v->sym = xmalloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

static struct lval *lval_sexpr(void)
{
	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

static struct lval *lval_qexpr(void)
{
	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

static struct lval *lval_builtin(lbuiltin func)
{
	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_FUN;
	v->builtin = func;
	return v;
}

static struct lval *lval_lambda(struct lval *formals, struct lval *body)
{
	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_FUN;
	v->builtin = NULL;

	v->env = lenv_new();
	v->formals = formals;
	v->body = body;
	return v;
}

static void lval_free(struct lval *v)
{
	int i;
	switch (v->type) {
	case LVAL_ERR:
		free(v->err);
		break;
	case LVAL_SYM:
		free(v->sym);
		break;
	case LVAL_CHARBUF:
		free(v->charbuf);
		break;
	case LVAL_SEXPR: /* fall through */
	case LVAL_QEXPR:
		for (i = 0; i < v->count; i++)
			lval_free(v->cell[i]);
		free(v->cell);
		break;
	case LVAL_FUN:
		if (!v->builtin) {
			lenv_free(v->env);
			lval_free(v->formals);
			lval_free(v->body);
		}
		break;
	case LVAL_NUM:
		break;
	}
	free(v);
}

static struct lval *lval_read_num(mpc_ast_t *t)
{
	errno = 0;
	long x = strtol(t->contents, NULL, 0);
	return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

static struct lval *lval_add(struct lval *v, struct lval *x)
{
	v->count++;
	v->cell = realloc(v->cell, sizeof(struct lval *) * v->count);
	v->cell[v->count - 1] = x;
	return v;
}

static struct lval *lval_read(mpc_ast_t *t)
{
	int i;

	/* return conversion to type */
	if (strstr(t->tag, "number"))
		return lval_read_num(t);
	if (strstr(t->tag, "symbol"))
		return lval_sym(t->contents);
	if (strstr(t->tag, "charbuf"))
		return lval_read_str(t);

	/* create empty list */
	struct lval *x = NULL;
	if (strcmp(t->tag, ">") == 0)
		x = lval_sexpr();
	if (strcmp(t->tag, "sexpr"))
		x = lval_sexpr();
	if (strstr(t->tag, "qexpr"))
		x = lval_qexpr();

	/* put expression in list */
	for (i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0)
			continue;
		if (strcmp(t->children[i]->contents, ")") == 0)
			continue;
		if (strcmp(t->children[i]->contents, "{") == 0)
			continue;
		if (strcmp(t->children[i]->contents, "}") == 0)
			continue;
		if (strcmp(t->children[i]->tag, "regex") == 0)
			continue;
		if (strstr(t->children[i]->tag, "comment"))
			continue;
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}

/* same as lval_expr_print, but return a char * with the contents */
static char *lval_expr_to_str(struct lenv *e, struct lval *v, char open,
			      char close)
{
	int i;
	char *new_buf, *old_buf;
	if (v->count == 0)
		return String("%c%c", open, close);

	/* initialize empty buffer for first concat */
	old_buf = xmalloc(1);
	old_buf[0] = '\0';

	/* append trailing space for all but last element */
	for (i = 0; i < v->count - 1; i++) {
		char *s = lval_to_str(e, v->cell[i]);
		new_buf = String("%s%s ", old_buf, s);
		free(old_buf);
		free(s);
		old_buf = new_buf;
	}
	char *s = lval_to_str(e, v->cell[i]);
	new_buf = String("%s%s", old_buf, s);
	free(old_buf);
	free(s);
	return String("%c%s%c", open, new_buf, close);
}

static void lval_expr_print(struct lenv *e, struct lval *v, char open,
			    char close)
{
	int i;
	putchar(open);
	if (v->count == 0)
		goto end;
	/* print trailing space for all but last element */
	for (i = 0; i < v->count - 1; i++) {
		lval_print(e, v->cell[i]);
		putchar(' ');
	}
	lval_print(e, v->cell[i]);
end:
	putchar(close);
}

/* Variadic heap allocated string builder. */
char *String(char *s, ...)
{
	int size = 512;
	char *buf = xmalloc(size);
	va_list va;
	va_start(va, s);

	/* Allocate 512 bytes of space */
	buf = malloc(512);

	/* printf the error string with a maximum of 511 characters */
	vsnprintf(buf, 511, s, va);

	/* Reallocate to number of bytes actually used */
	buf = realloc(buf, strlen(buf) + 1);

	return buf;
}

static char *fmt(const char *fmt, ...)
{
	const int size = 512;
	va_list va;
	va_start(va, fmt);
	void *buf = xmalloc(size);
	vsnprintf(buf, size, fmt, va);
	va_end(va);
	return buf;
}

static char *lval_str_to_str(struct lval *v)
{
	char *escaped = xmalloc(strlen(v->charbuf) + 1);
	strcpy(escaped, v->charbuf);
	escaped = mpcf_escape(escaped);
	return escaped;
}

static void lval_print_str(struct lval *v)
{
	char *escaped = lval_str_to_str(v);
	printf("\"%s\"", escaped);
	free(escaped);
}

/* returns a str representation of an lval for internal usage
 *
 * @param v: lval to convert
 * @return: char * representation of lval
 */
static char *lval_to_str(struct lenv *e, struct lval *v)
{
	switch (v->type) {
	case LVAL_NUM:
		return String("%li", v->num);
	case LVAL_ERR:
		return String("\nErr: %s", v->err);
	case LVAL_SYM:
		return String("%s", v->sym);
	case LVAL_SEXPR:
		return lval_expr_to_str(e, v, '(', ')');
	case LVAL_CHARBUF:
		return lval_str_to_str(v);
	case LVAL_QEXPR:
		return lval_expr_to_str(e, v, '{', '}');
	case LVAL_FUN:
		if (v->builtin)
			return String("<builtin function '%s'>",
				      lenv_lookup_sym_by_val(e, v));
		else {
			char *formals = lval_to_str(e, v->formals);
			char *body = lval_to_str(e, v->body);
			char *out = String("%s %s %s)",
					   lenv_lookup_sym_by_val(e, v),
					   formals, body);
			free(formals);
			free(body);
			return out;
		}
		break;
	}
	return String("Unknown lval type!");
}

static void lval_print(struct lenv *e, struct lval *v)
{
	switch (v->type) {
	case LVAL_NUM:
		printf("%li", v->num);
		break;
	case LVAL_ERR:
		printf("\nErr: %s", v->err);
		break;
	case LVAL_SYM:
		printf("%s", v->sym);
		break;
	case LVAL_CHARBUF:
		lval_print_str(v);
		break;
	case LVAL_SEXPR:
		lval_expr_print(e, v, '(', ')');
		break;
	case LVAL_QEXPR:
		lval_expr_print(e, v, '{', '}');
		break;
	case LVAL_FUN:
		if (v->builtin)
			printf("<builtin function '%s'>",
			       lenv_lookup_sym_by_val(e, v));
		else {
			printf("%s", lenv_lookup_sym_by_val(e, v));
			lval_print(e, v->formals);
			putchar(' ');
			lval_print(e, v->body);
			putchar(')');
		}
		break;
	default:
		printf("Unknown lval type!");
		break;
	}
}

static void lval_println(struct lenv *e, struct lval *v)
{
	lval_print(e, v);
	putchar('\n');
}

struct lval *lval_eval_sexpr(struct lenv *e, struct lval *v)
{
	int i;
	const char fname[] = "eval_sexpr";

	/* eval children*/
	for (i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(e, v->cell[i]);
	}

	/* error check */
	for (i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR)
			return lval_take(v, i);
	}

	/* empty expressions */
	if (v->count == 0)
		return v;

	/* single expressions */
	if (v->count == 1)
		return lval_take(v, 0);

	/* ensure first elem is func */
	struct lval *f = lval_pop(v, 0);
	if (f->type != LVAL_FUN) {
		lval_free(f);
		lval_free(v);
		return lerr_args_type(f, fname, LVAL_FUN, f->type);
	}

	/* builtin */
	struct lval *result = lval_call(e, f, v);
	lval_free(f);
	return result;
}

struct lval *lval_join_qexpr(struct lval *x, struct lval *y)
{
	while (y->count)
		x = lval_add(x, lval_pop(y, 0));
	free(y);
	return x;
}

struct lval *lval_join_charbuf(struct lenv *e, struct lval *a)
{
	int space = 0;
	int i;
	for (i = 0; i < a->count; i++) {
		space += strlen(a->cell[i]->charbuf);
	}
	char *buf = xmalloc(space + 1);
	buf[0] = '\0';
	for (i = 0; i < a->count; i++) {
		strcat(buf, a->cell[i]->charbuf);
	}
	struct lval *out = lval_str(buf);
	free(buf);
	free(a);
	return out;
}

struct lval *lval_eval(struct lenv *e, struct lval *v)
{
	if (v->type == LVAL_SYM) {
		struct lval *x = lenv_get(e, v);
		lval_free(v);
		return x;
	}
	if (v->type == LVAL_SEXPR)
		return lval_eval_sexpr(e, v);
	return v;
}

struct lval *lval_pop(struct lval *v, int i)
{
	struct lval *x = v->cell[i];
	memmove(&v->cell[i], &v->cell[i + 1],
		sizeof(struct lval *) * (v->count - i - 1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(struct lval *) * v->count);
	return x;
}

struct lval *lval_take(struct lval *v, int i)
{
	struct lval *x = lval_pop(v, i);
	lval_free(v);
	return x;
}

struct lval *lval_func_err(struct lval *a, const char *fname,
			   const char *message, ...)
{
	const char format[] = "Function '%s' %s";
	const int size = 512;

	va_list va;
	va_start(va, message);
	char *concat = fmt(format, fname, message);

	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_ERR;

	v->err = xmalloc(size);
	vsnprintf(v->err, size, concat, va);
	va_end(va);
	free(concat);
	return v;
}

static struct lval *lval_copy(struct lval *v)
{
	struct lval *x = xmalloc(sizeof(struct lval));

	x->type = v->type;

	switch (v->type) {
	/* copy direct */
	case LVAL_FUN:
		if (v->builtin)
			x->builtin = v->builtin;
		else {
			x->builtin = NULL;
			x->env = lenv_copy(v->env);
			x->formals = lval_copy(v->formals);
			x->body = lval_copy(v->body);
		}
		break;
	case LVAL_NUM:
		x->num = v->num;
		break;

	/* copy strings */
	case LVAL_ERR:
		x->err = xmalloc(strlen(v->err) + 1);
		strcpy(x->err, v->err);
		break;
	case LVAL_SYM:
		x->sym = xmalloc(strlen(v->sym) + 1);
		strcpy(x->sym, v->sym);
		break;
	case LVAL_CHARBUF:
		x->sym = xmalloc(strlen(v->charbuf) + 1);
		strcpy(x->charbuf, v->charbuf);
		break;

	/* copy lists */
	case LVAL_SEXPR: /*fallthrough*/
	case LVAL_QEXPR: {
		int i;
		x->count = v->count;
		x->cell = xmalloc(sizeof(struct lval *) * x->count);
		for (i = 0; i < x->count; i++) {
			x->cell[i] = lval_copy(v->cell[i]);
		}
		break;
	}
	default:
		lval_err("Unknown lval type!");
	}
	return x;
}

/* lenv funcs */
static struct lenv *lenv_new(void)
{
	struct lenv *e = xmalloc(sizeof(struct lenv));
	e->par = NULL;
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

static void lenv_free(struct lenv *e)
{
	int i;
	for (i = 0; i < e->count; i++) {
		free(e->syms[i]);
		lval_free(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

static int lenv_get_val_pos(struct lenv *e, struct lval *a)
{
	int i;
	for (i = 0; i < e->count; i++) {
		if (lval_eq(e->vals[i], a))
			return i;
	}
	return -1;
}

static int lenv_get_sym_pos(struct lenv *e, char *sym)
{
	int i;
	for (i = 0; i < e->count; i++) {
		if (strcmp(e->syms[i], sym) == 0)
			return i;
	}
	return -1;
}

static struct lval *lenv_get(struct lenv *e, struct lval *l)
{
	int pos = lenv_get_sym_pos(e, l->sym);
	if (pos != -1)
		return lval_copy(e->vals[pos]);
	else if (e->par)
		return lenv_get(e->par, l);
	return lval_err("unbound symbol '%s'", l->sym);
}

static struct lenv *lenv_copy(struct lenv *e)
{
	int i;
	struct lenv *n = malloc(sizeof(struct lenv));
	n->par = e->par;
	n->count = e->count;
	n->syms = malloc(sizeof(char *) * n->count);
	n->vals = malloc(sizeof(struct lval *) * n->count);
	for (i = 0; i < e->count; i++) {
		n->syms[i] = malloc(strlen(e->syms[i]) + 1);
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = lval_copy(e->vals[i]);
	}
	return n;
}

/* Add "variable" (k/v pair) to the local environment
 *
 * @param e: environment to put variables in
 * @param k: lval of variable names
 * @param v: lval of values
 */
static void lenv_put(struct lenv *e, struct lval *k, struct lval *v)
{
	int i = lenv_get_sym_pos(e, k->sym);

	/* exists, replace */
	if (i != -1) {
		lval_free(e->vals[i]);
		e->vals[i] = lval_copy(v);
		return;
	}

	/* does not exist, create*/
	e->count++;
	e->vals = realloc(e->vals, sizeof(struct lval *) * e->count);
	e->syms = realloc(e->syms, sizeof(char *) * e->count);

	/* copy */
	e->vals[e->count - 1] = lval_copy(v);
	e->syms[e->count - 1] = xmalloc(strlen(k->sym) + 1);
	strcpy(e->syms[e->count - 1], k->sym);
}

/* Add "variable" (k/v pair) to the global environment
 *
 * @param e: environment to find global from
 * @param k: lval of variable names
 * @param v: lval of values
 */
static void lenv_def(struct lenv *e, struct lval *k, struct lval *v)
{
	while (e->par)
		e = e->par;
	lenv_put(e, k, v);
}

static void lenv_add_builtin(struct lenv *e, char *name, lbuiltin func)
{
	struct lval *k = lval_sym(name);
	struct lval *v = lval_builtin(func);
	lenv_put(e, k, v);
	lval_free(k);
	lval_free(v);
}

static void lenv_add_builtins(struct lenv *e)
{
	lenv_add_builtin(e, "load", builtin_load);
	lenv_add_builtin(e, "type", builtin_type);
	lenv_add_builtin(e, "error", builtin_error);
	lenv_add_builtin(e, "print", builtin_print);

	/* list functions */
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);
	lenv_add_builtin(e, "def", builtin_def);
	lenv_add_builtin(e, "put", builtin_put);
	lenv_add_builtin(e, "\\", builtin_lambda);

	/* math functions */
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
	lenv_add_builtin(e, "%", builtin_mod);

	/* conditional */
	lenv_add_builtin(e, "if", builtin_if);

	/* equality */
	lenv_add_builtin(e, "==", builtin_eq);
	lenv_add_builtin(e, "!=", builtin_ne);

	/* order */
	lenv_add_builtin(e, "<=", builtin_le);
	lenv_add_builtin(e, "<", builtin_lt);
	lenv_add_builtin(e, ">", builtin_gt);
	lenv_add_builtin(e, ">=", builtin_ge);

	/* logical */
	lenv_add_builtin(e, "&&", builtin_and);
	lenv_add_builtin(e, "||", builtin_or);
	lenv_add_builtin(e, "!", builtin_not);

	/* bitwise */
	lenv_add_builtin(e, "~", builtin_bitwise_not);
	lenv_add_builtin(e, "|", builtin_bitwise_or);
	lenv_add_builtin(e, "&", builtin_bitwise_and);
	lenv_add_builtin(e, "^", builtin_bitwise_xor);
	lenv_add_builtin(e, ">>", builtin_bitwise_right_shift);
	lenv_add_builtin(e, "<<", builtin_bitwise_left_shift);

	/* testing */
	lenv_add_builtin(e, "assert", builtin_assert);
}

static struct lval *lenv_load(struct lenv *e, char *file)
{
	/* parse file of string name */
	mpc_result_t r;
	if (mpc_parse_contents(file, Lisp, &r)) {
		struct lval *expr = lval_read(r.output);
		int i = 1;
		mpc_ast_delete(r.output);
		while (expr->count) {
			struct lval *x = lval_eval(e, lval_pop(expr, 0));
			if (x->type == LVAL_ERR) {
				printf("\nLoad error in %s:%d\b", file, i);
				lval_println(e, x);
			}
			lval_free(x);
			i++;
		}
		lval_free(expr);
		return lval_sexpr();
	} else {
		/* get parser error as string */
		char *err_msg = mpc_err_string(r.error);
		mpc_err_delete(r.error);

		struct lval *err = lval_err("Could not load %s", err_msg);
		free(err_msg);
		return err;
	}
}

static char *lenv_lookup_sym_by_val(struct lenv *e, struct lval *a)
{
	int i = lenv_get_val_pos(e, a);
	if (i != -1)
		return e->syms[i];
	else if (e->par)
		return lenv_lookup_sym_by_val(e->par, a);
	return "\\";
}

static char *lval_args_to_str(struct lenv *e, struct lval *a)
{
	char *old_buf, *new_buf;

	/* initialize empty buffer for first concat */
	old_buf = xmalloc(1);
	old_buf[0] = '\0';

	/* append trailing space for all but last element */
	while (a->count) {
		struct lval *x = lval_pop(a, 0);
		char *s = lval_to_str(e, x);
		new_buf = String("%s%s", old_buf, s);
		free(old_buf);
		free(s);
		free(x);
		old_buf = new_buf;
	}
	return old_buf;
}

static struct lval *builtin_print(struct lenv *e, struct lval *a)
{
	int i;
	for (i = 0; i < a->count; i++) {
		lval_print(e, a->cell[i]);
		putchar(' ');
	}
	putchar('\n');
	lval_free(a);
	return lval_sexpr();
}

static struct lval *builtin_error(struct lenv *e, struct lval *a)
{
	char fname[] = "error";
	if (a->count < 1)
		return lerr_args_too_few_variable(a, fname, 1);

	char *msg = lval_args_to_str(e, a);
	struct lval *err = lval_err(msg);
	free(msg);
	return err;
}

static struct lval *builtin_op(struct lenv *e, struct lval *a, char *fname,
			       char *op)
{
	int i;
	for (i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			struct lval *out = lval_func_err(
				a, fname,
				"Cannot operate on non-number. Type %s",
				ltype_name(a->cell[i]->type));
			lval_free(a);
			return out;
		}
	}

	struct lval *x = lval_pop(a, 0);

	/* unary negation */
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->num = -x->num;
	}

	/* TODO: obvious algorithmic performance opportunities here */
	while (a->count > 0) {
		struct lval *y = lval_pop(a, 0);

		if (strcmp(op, "+") == 0)
			x->num += y->num;
		if (strcmp(op, "-") == 0)
			x->num -= y->num;
		if (strcmp(op, "*") == 0)
			x->num *= y->num;
		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				lval_free(x);
				lval_free(y);
				x = lval_err("Division By Zero!");
				break;
			}
			x->num /= y->num;
		}
		if (strcmp(op, "%") == 0) {
			if (y->num == 0) {
				lval_free(x);
				lval_free(y);
				x = lval_err("Modulo By Zero!");
				break;
			}
			x->num %= y->num;
		}
		lval_free(y);
	}
	lval_free(a);
	return x;
}

/* Evaluate and compare both arguments for equality.
 * @param e: env
 * @param a: lval containing two args for comparison
 */
static struct lval *builtin_assert(struct lenv *e, struct lval *a)
{
	if (a->count != 2)
		return lerr_args_num(a, "assert", 2);

	struct lval *r1 = lval_eval(e, a->cell[0]);
	struct lval *r2 = lval_eval(e, a->cell[1]);
	if (lval_eq(r1, r2)) {
		free(a);
		return lval_num(1);
	} else {
		char *expr1 = lval_to_str(e, a->cell[0]);
		char *expr2 = lval_to_str(e, a->cell[1]);
		struct lval *out =
			lval_err("assert failed [%s] != [%s]", expr1, expr2);

		free(a);
		return out;
	}
}

static struct lval *builtin_type(struct lenv *e, struct lval *a)
{
	char fname[] = "type";
	if (a->count != 1)
		return lerr_args_num(a, fname, 1);
	return lval_str(ltype_name(a->cell[0]->type));
}

static struct lval *builtin_load(struct lenv *e, struct lval *a)
{
	char fname[] = "load";
	if (a->count != 1) {
		return lerr_args_num(a, fname, 1);
	}
	if (a->cell[0]->type != LVAL_CHARBUF) {
		return lerr_args_type(a, fname, LVAL_CHARBUF, a->type);
	}
	return lenv_load(e, a->cell[0]->charbuf);
}

/* builtin math ops */
static struct lval *builtin_add(struct lenv *e, struct lval *a)
{
	return builtin_op(e, a, "add", "+");
}

static struct lval *builtin_sub(struct lenv *e, struct lval *a)
{
	return builtin_op(e, a, "sub", "-");
}

static struct lval *builtin_mul(struct lenv *e, struct lval *a)
{
	return builtin_op(e, a, "mul", "*");
}

static struct lval *builtin_div(struct lenv *e, struct lval *a)
{
	return builtin_op(e, a, "div", "/");
}

static struct lval *builtin_mod(struct lenv *e, struct lval *a)
{
	return builtin_op(e, a, "mod", "%");
}

static struct lval *builtin_def(struct lenv *e, struct lval *a)
{
	return builtin_var(e, a, "def");
}

static struct lval *builtin_put(struct lenv *e, struct lval *a)
{
	return builtin_var(e, a, "=");
}

static struct lval *builtin_eq(struct lenv *e, struct lval *a)
{
	if (a->count < 2)
		return lerr_args_too_few_variable(a, "==", 2);
	int i;
	int ret;
	for (i = 0; i < a->count; i++) {
		ret = lval_eq(a->cell[i], a->cell[i + 1]);
		if (ret == 0)
			break;
	}
	lval_free(a);
	return lval_num(ret);
}

static struct lval *builtin_ne(struct lenv *e, struct lval *a)
{
	if (a->count != 2)
		return lerr_args_num(a, "!=", 2);
	int ret = !lval_eq(a->cell[0], a->cell[1]);
	lval_free(a);
	return lval_num(ret);
}

static struct lval *builtin_gt(struct lenv *e, struct lval *a)
{
	return builtin_order(e, a, ">");
}

static struct lval *builtin_ge(struct lenv *e, struct lval *a)
{
	return builtin_order(e, a, ">=");
}

static struct lval *builtin_lt(struct lenv *e, struct lval *a)
{
	return builtin_order(e, a, "<");
}

static struct lval *builtin_le(struct lenv *e, struct lval *a)
{
	return builtin_order(e, a, "<=");
}

static struct lval *builtin_order(struct lenv *e, struct lval *a, char *op)
{
	if (a->count != 2)
		return lerr_args_num(a, op, 2);
	if (a->cell[0]->type != LVAL_NUM)
		return lerr_args_type(a, op, LVAL_NUM, a->type);
	if (a->cell[1]->type != LVAL_NUM)
		return lerr_args_type(a, op, LVAL_NUM, a->type);
	int num1 = a->cell[0]->num;
	int num2 = a->cell[1]->num;
	int ret;
	if (strcmp(">", op) == 0) {
		ret = (num1 > num2);
	} else if (strcmp(">=", op) == 0) {
		ret = (num1 >= num2);
	} else if (strcmp("<", op) == 0) {
		ret = (num1 < num2);
	} else if (strcmp("<=", op) == 0) {
		ret = (num1 <= num2);
	} else {
		return lerr_arg_error(
			a, op,
			"comparison op is not a member of set(>, >=, <, <=)");
	}
	lval_free(a);
	return lval_num(ret);
}

static struct lval *builtin_logical(struct lenv *e, struct lval *a, char *op)
{
	int i;
	for (i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			struct lval *out = lval_func_err(
				a, op, "Cannot operate on non-number. Type %s",
				ltype_name(a->cell[i]->type));
			lval_free(a);
			return out;
		}
	}

	struct lval *x = lval_pop(a, 0);

	/* not */
	if (strcmp(op, "!") == 0) {
		if (a->count == 0) {
			x->num = !x->num;
		} else {
			struct lval *err = lerr_args_too_many(a, op, 0);
			lval_free(a);
			return err;
		}
	}

	/* TODO: obvious algorithmic performance opportunities here */
	while (a->count > 0) {
		struct lval *y = lval_pop(a, 0);

		if (strcmp(op, "&&") == 0)
			x->num = x->num && y->num;
		if (strcmp(op, "||") == 0)
			x->num = x->num || y->num;
		lval_free(y);
	}
	lval_free(a);
	return x;
}

static struct lval *builtin_bitwise(struct lenv *e, struct lval *a, char *op)
{
	int i;
	for (i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			struct lval *out = lval_func_err(
				a, op, "Cannot operate on non-number. Type %s",
				ltype_name(a->cell[i]->type));
			lval_free(a);
			return out;
		}
	}

	struct lval *x = lval_pop(a, 0);

	/* one's complement negation */
	if ((strcmp(op, "~") == 0)) {
		if (a->count == 0) {
			x->num = ~x->num;
		} else {
			struct lval *err = lerr_args_too_many(a, op, 0);
			lval_free(a);
			return err;
		}
	}

	/* TODO: obvious algorithmic performance opportunities here */
	while (a->count > 0) {
		struct lval *y = lval_pop(a, 0);

		if (strcmp(op, "&") == 0)
			x->num = x->num & y->num;
		if (strcmp(op, "|") == 0)
			x->num = x->num | y->num;
		if (strcmp(op, "<<") == 0)
			x->num = x->num << y->num;
		if (strcmp(op, ">>") == 0)
			x->num = x->num >> y->num;
		if (strcmp(op, "^") == 0)
			x->num = x->num ^ y->num;
		lval_free(y);
	}
	lval_free(a);
	return x;
}

static struct lval *builtin_bitwise_and(struct lenv *e, struct lval *a)
{
	return builtin_bitwise(e, a, "&");
}
static struct lval *builtin_bitwise_or(struct lenv *e, struct lval *a)
{
	return builtin_bitwise(e, a, "|");
}
static struct lval *builtin_bitwise_not(struct lenv *e, struct lval *a)
{
	return builtin_bitwise(e, a, "~");
}
static struct lval *builtin_bitwise_left_shift(struct lenv *e, struct lval *a)
{
	return builtin_bitwise(e, a, "<<");
}
static struct lval *builtin_bitwise_right_shift(struct lenv *e, struct lval *a)
{
	return builtin_bitwise(e, a, ">>");
}
static struct lval *builtin_bitwise_xor(struct lenv *e, struct lval *a)
{
	return builtin_bitwise(e, a, "^");
}

static struct lval *builtin_or(struct lenv *e, struct lval *a)
{
	return builtin_logical(e, a, "||");
}
static struct lval *builtin_and(struct lenv *e, struct lval *a)
{
	return builtin_logical(e, a, "&&");
}
static struct lval *builtin_not(struct lenv *e, struct lval *a)
{
	return builtin_logical(e, a, "!");
}

static int lval_eq(struct lval *x, struct lval *y)
{
	int i;
	if (x->type != y->type)
		return 0;
	switch (x->type) {
	case LVAL_FUN:
		if (x->builtin || y->builtin)
			return x->builtin == y->builtin;
		else {
			return lval_eq(x->formals, y->formals) &&
			       lval_eq(x->body, y->body);
		}
	case LVAL_NUM:
		return x->num == y->num;
	case LVAL_ERR:
		return strcmp(x->err, y->err) == 0;
	case LVAL_SYM:
		return strcmp(x->sym, y->sym) == 0;
	case LVAL_CHARBUF:
		return strcmp(x->charbuf, y->charbuf) == 0;
	case LVAL_SEXPR: /* fallthrough */
	case LVAL_QEXPR:
		if (x->count != y->count)
			return 0;
		for (i = 0; i < x->count; i++) {
			if (!lval_eq(x->cell[i], y->cell[i]))
				return 0;
		}
		return 1;
	default:
		return 0;
	}
}

static struct lval *lerr_args_num(struct lval *a, const char *fname,
				  int expected)
{
	if (expected > a->count)
		return lerr_args_too_few(a, fname, expected);
	else
		return lerr_args_too_many(a, fname, expected);
}

static struct lval *lerr_args_num_desc(struct lval *a, const char *fname,
				       char *desc, int expected, int received)
{
	return lval_func_err(a, fname,
			     "passed %s arguments. Got %d, expected %d", desc,
			     received, expected);
}

static struct lval *lerr_args_too_many(struct lval *a, const char *fname,
				       int expected)
{
	return lerr_args_num_desc(a, fname, "too many", expected, a->count);
}

static struct lval *lerr_args_too_many_variable(struct lval *a, const char *fname,
				       int max)
{
	return lval_func_err(a, fname, "passed too many arguments. Expected"
			" no more than %d arguments", max);
}

static struct lval *lerr_args_too_few(struct lval *a, const char *fname,
				      int expected)
{
	return lerr_args_num_desc(a, fname, "too few", expected, a->count);
}

static struct lval *lerr_args_too_few_variable(struct lval *a, const char *fname,
				      int min)
{
	return lval_func_err(a, fname, "passed too few arguments. Expected"
			" %d or more arguments", min);
}

static struct lval *lerr_args_type(struct lval *a, const char *fname,
				   int expected, int received)
{
	return lval_func_err(a, fname,
			     "passed incorrect type. Got %s, expected %s",
			     ltype_name(received), ltype_name(expected));
}

static struct lval *lerr_args_mult_type(struct lval *a, const char *fname,
					int type1, int type2)
{
	return lval_func_err(
		a, fname,
		"passed multiple types. Expected one type but receive %s and %s",
		ltype_name(type1), ltype_name(type2));
}

static struct lval *lerr_arg_error(struct lval *a, const char *fname, char *msg)
{
	return lval_func_err(a, fname, msg);
}

/* builtin q-expr funcs */
static struct lval *builtin_head(struct lenv *e, struct lval *a)
{
	const char fname[] = "head";
	if (a->count != 1)
		return lerr_args_too_many(a, fname, 1);
	if (a->cell[0]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, LVAL_QEXPR, a->cell[0]->type);
	if (a->cell[0]->count == 0)
		return lval_func_err(a, fname, "passed {}");

	struct lval *v = lval_take(a, 0);
	while (v->count > 1)
		lval_free(lval_pop(v, 1));
	return v;
}

static struct lval *builtin_tail(struct lenv *e, struct lval *a)
{
	const char fname[] = "tail";
	if (a->count != 1)
		return lerr_args_too_many(a, fname, 1);
	if (a->cell[0]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, a->cell[0]->type, LVAL_QEXPR);
	if (a->cell[0]->count == 0)
		return lval_func_err(a, fname, "passed {}");

	struct lval *v = lval_take(a, 0);
	lval_free(lval_pop(v, 0));
	return v;
}

static struct lval *builtin_list(struct lenv *e, struct lval *a)
{
	a->type = LVAL_QEXPR;
	return a;
}

static struct lval *builtin_eval(struct lenv *e, struct lval *a)
{
	const char fname[] = "eval";
	if (a->count != 1)
		return lerr_args_too_many(a, fname, 1);
	if (a->cell[0]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, a->cell[0]->type, LVAL_QEXPR);

	struct lval *x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}

/* (if condition {execute if cond true} {execute if cond false})
 *
 */
static struct lval *builtin_if(struct lenv *e, struct lval *a)
{
	const char fname[] = "eval";
	if (a->count != 3)
		return lerr_args_num(a, fname, 3);
	if (a->cell[0]->type != LVAL_NUM)
		return lerr_args_type(a, fname, LVAL_NUM, a->cell[0]->type);
	if (a->cell[1]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, LVAL_QEXPR, a->cell[1]->type);
	if (a->cell[2]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, LVAL_QEXPR, a->cell[2]->type);

	struct lval *x;
	a->cell[1]->type = LVAL_SEXPR;
	a->cell[2]->type = LVAL_SEXPR;

	/* conditionally execute the first or second qexpr*/
	if (a->cell[0]->num)
		x = lval_eval(e, lval_pop(a, 1));
	else
		x = lval_eval(e, lval_pop(a, 2));

	lval_free(a);
	return x;
}

struct lval *builtin_join(struct lenv *e, struct lval *a)
{
	int i;
	const char fname[] = "join";
	int last_type = a->cell[0]->type;
	for (i = 0; i < a->count; i++) {
		if (a->cell[i]->type == LVAL_QEXPR) {
			if (last_type != LVAL_QEXPR)
				return lerr_args_mult_type(a, fname, last_type,
							   LVAL_QEXPR);
		} else if (a->cell[i]->type == LVAL_CHARBUF) {
			if (last_type != LVAL_CHARBUF)
				return lerr_args_mult_type(a, fname, last_type,
							   LVAL_CHARBUF);
		} else {
			return lval_func_err(
				a, fname,
				"Function %s: expected argument types in set(%s, %s), received: %s",
				fname, ltype_name(LVAL_QEXPR),
				ltype_name(LVAL_QEXPR),
				ltype_name(a->cell[i]->type));
		}
	}

	struct lval *x;
	if (a->cell[0]->type == LVAL_QEXPR) {
		x = lval_pop(a, 0);
		while (a->count)
			x = lval_join_qexpr(x, lval_pop(a, 0));
	} else if (a->cell[0]->type == LVAL_CHARBUF) {
		x = lval_join_charbuf(e, a);
	} else {
		return lval_func_err(
			a, fname,
			"Function %s: expected argument types in set(%s, %s), received: %s",
			fname, ltype_name(LVAL_QEXPR), ltype_name(LVAL_QEXPR),
			ltype_name(a->cell[0]->type));
	}

	return x;
}

/*
 * Verify first cell in lval is a qexpr of symbols
 *
 * Used for checking variable and function definitions.
 *
 * @param a: lval to check
 * @param fname: to insert in returned error
 * @return: NULL if check passes, else LVAL_ERR
 */
static struct lval *lerr_verify_first_arg_is_qexpr_of_symbols(struct lval *a,
							      const char *fname)
{
	int i;
	/* first arg is symbol list */
	struct lval *syms = a->cell[0];
	if (a->cell[0]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, LVAL_QEXPR, a->cell[0]->type);

	for (i = 0; i < syms->count; i++)
		if (syms->cell[i]->type != LVAL_SYM)
			return lerr_args_type(a, fname, LVAL_SYM,
					      syms->cell[i]->type);
	return NULL;
}

/* def builtin */
static struct lval *builtin_var(struct lenv *e, struct lval *a, char *fname)
{
	int i;
	struct lval *err = lerr_verify_first_arg_is_qexpr_of_symbols(a, fname);
	if (err != NULL)
		return err;

	/* first arg is qexpr of symbols */
	struct lval *syms = a->cell[0];

	if (syms->count != a->count - 1) {
		char *expected_syms = lval_to_str(e, syms);
		struct lval *tmp = lval_func_err(
			a, fname,
			"Number of symbols must match number of values.\n\n"
			"\tlen(%s) == (%d) symbol(s) given but %d values given",
			expected_syms, syms->count, a->count - 1);
		free(expected_syms);
		return tmp;
	}

	/* assign copies of values to symbols */

	if (strcmp(fname, "def") == 0)
		for (i = 0; i < syms->count; i++)
			lenv_def(e, syms->cell[i], a->cell[i + 1]);

	if (strcmp(fname, "=") == 0)
		for (i = 0; i < syms->count; i++)
			lenv_put(e, syms->cell[i], a->cell[i + 1]);

	lval_free(a);
	return lval_sexpr();
}

static struct lval *builtin_lambda(struct lenv *e, struct lval *a)
{
	const char fname[] = "\\";
	int arg2_type = a->cell[1]->type;
	if (a->count != 2)
		return lerr_args_num(a, fname, 2);

	struct lval *err = lerr_verify_first_arg_is_qexpr_of_symbols(a, fname);
	if (err != NULL)
		return err;

	if (arg2_type != LVAL_QEXPR)
		return lerr_args_type(a, fname, LVAL_QEXPR, arg2_type);

	/* pop first two args and pass them to lval_lambda */
	struct lval *formals = lval_pop(a, 0);
	struct lval *body = lval_pop(a, 0);
	lval_free(a);
	return lval_lambda(formals, body);
}

char *ltype_name(int t)
{
	switch (t) {
	case LVAL_FUN:
		return "Function";
	case LVAL_NUM:
		return "Number";
	case LVAL_ERR:
		return "Error";
	case LVAL_SYM:
		return "Symbol";
	case LVAL_CHARBUF:
		return "Charbuf";
	case LVAL_SEXPR:
		return "S-Expression";
	case LVAL_QEXPR:
		return "Q-Expression";
	default:
		return "Unknown";
	}
}

int main(int argc, char *argv[])
{
	int i;
	puts(version);

	/* Create Some Parsers */
	Number = mpc_new("number");
	Symbol = mpc_new("symbol");
	Charbuf = mpc_new("charbuf");
	Comment = mpc_new("comment");
	Sexpr = mpc_new("sexpr");
	Qexpr = mpc_new("qexpr");
	Expr = mpc_new("expr");
	Lisp = mpc_new("lisp");

	/* Define them with the following Language */
	mpca_lang(MPCA_LANG_DEFAULT,
		  "                                                 \
		number   : /-?[0-9]+/ ;                             \
		charbuf  : /\"(\\\\.|[^\"])*\"/ ;                   \
		comment  : /;[^\\r\\n]*/ ;                          \
		symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&%^~|]+/ ;   \
		sexpr    : '(' <expr>* ')' ;                        \
		qexpr    : '{' <expr>* '}' ;                        \
		expr     : <number> | <symbol> | <charbuf> |        \
		           <comment> | <sexpr> | <qexpr> ;          \
		lisp    : /^/ <expr>* /$/ ;                         \
		",
		  Number, Charbuf, Comment, Symbol, Sexpr, Qexpr, Expr, Lisp);
	struct lenv *e = lenv_new();
	lenv_add_builtins(e);

	if (argc >= 2) {
		/* execute file(s) */
		for (i = 1; i < argc; i++) {
			struct lval *args =
				lval_add(lval_sexpr(), lval_str(argv[i]));
			struct lval *x = builtin_load(e, args);

			if (x->type == LVAL_ERR)
				lval_println(e, x);
			lval_free(x);
		}

	} else {
		/* repl loop */
		puts("Press Ctrl+c to Exit\n");
		struct lval *x = lenv_load(e, "./lsp/lib.lsp");
		if (x->type == LVAL_ERR)
			lval_println(e, x);
		lval_free(x);
		while (1) {
			mpc_result_t r;
			char *input = readline("lisp> ");
			add_history(input);
			if (mpc_parse("<stdin>", input, Lisp, &r)) {
				struct lval *x =
					lval_eval(e, lval_read(r.output));
				lval_println(e, x);
				mpc_ast_delete(r.output);
				lval_free(x);
			} else {
				mpc_err_print(r.error);
				mpc_err_delete(r.error);
			}
		}
	}
	lenv_free(e);
	mpc_cleanup(8, Number, Symbol, Charbuf, Comment, Sexpr, Qexpr, Expr,
		    Lisp);
	return 0;
}
