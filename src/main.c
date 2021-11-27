#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <editline/readline.h>
#include <editline/history.h>

#include <mpc.h>

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
	LVAL_FUN,
	LVAL_SEXPR,
	LVAL_QEXPR,
};

enum {
	LERR_DIV_ZERO,
	LERR_BAD_OP,
	LERR_BAD_NUM,
};

struct lval;
struct lenv;

typedef struct lval *(*lbuiltin)(struct lenv *, struct lval *);

struct lval {
	int type;

	union {
		/* basic */
		struct {
			long num;
			char *err;
			char *sym;
		};

		/* Function */
		struct {
			lbuiltin fun;
			struct lenv *env;
			struct lenv *formals;
			struct lenv *body;
		};

		/* Expression */
		struct {
			int count;
			struct lval **cell;
		};
	};
};

struct lenv {
	int count;
	char **syms;
	struct lval **vals;
};

char *String(char *s, ...);
static void lval_expr_print(struct lval *, char open, char close);
static char *lval_expr_to_str(struct lval *, char open, char close);
static char *lval_to_str(struct lval *v);
static void lval_print(struct lval *);
static struct lval *lval_eval(struct lenv *, struct lval *);
static struct lval *lval_take(struct lval *, int i);
static struct lval *lval_pop(struct lval *, int i);
static struct lval *lenv_get(struct lenv *, struct lval *);
static struct lval *lval_err(const char *fmt, ...);
static struct lval *builtin_op(struct lenv *, struct lval *, char *, char *);
static struct lval *builtin_eval(struct lenv *, struct lval *);
static struct lval *builtin_join(struct lenv *, struct lval *);
static struct lval *builtin_list(struct lenv *, struct lval *);
static struct lval *builtin_head(struct lenv *, struct lval *);
static struct lval *builtin_tail(struct lenv *, struct lval *);
static struct lval *builtin_add(struct lenv *, struct lval *);
static struct lval *builtin_sub(struct lenv *, struct lval *);
static struct lval *builtin_mul(struct lenv *, struct lval *);
static struct lval *builtin_div(struct lenv *, struct lval *);
static struct lval *builtin_def(struct lenv *, struct lval *);
static char *ltype_name(int t);

/* lval constructors */
static struct lval *lval_num(long x)
{
	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
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

static struct lval *lval_fun(lbuiltin func)
{
	struct lval *v = xmalloc(sizeof(struct lval));
	v->type = LVAL_FUN;
	v->fun = func;
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
	case LVAL_SEXPR: /* fall through */
	case LVAL_QEXPR:
		for (i = 0; i < v->count; i++)
			lval_free(v->cell[i]);
		free(v->cell);
		break;
	case LVAL_FUN: /* fall through */
	case LVAL_NUM:
		break;
	}
	free(v);
}

static struct lval *lval_read_num(mpc_ast_t *t)
{
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
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
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}

/* same as lval_expr_print, but return a char * with the contents */
static char *lval_expr_to_str(struct lval *v, char open, char close)
{
	int i;
	char *new_buf, *old_buf;
	if (v->count == 0)
		return String("%c%c", open, close);

	/* initialize empty buffer for first concat*/
	old_buf = xmalloc(1);
	old_buf[0] = '\0';

	/* append trailing space for all but last element */
	for (i = 0; i < v->count - 1; i++) {
		char *s = lval_to_str(v->cell[i]);
		new_buf = String("%s%s ", old_buf, s);
		free(old_buf);
		free(s);
		old_buf = new_buf;
	}
	char *s = lval_to_str(v->cell[i]);
	new_buf = String("%s%s", old_buf, s);
	free(old_buf);
	free(s);
	return String("%c%s%c", open, new_buf, close);
}

static void lval_expr_print(struct lval *v, char open, char close)
{
	int i;
	putchar(open);
	if (v->count == 0)
		goto end;
	/* print trailing space for all but last element */
	for (i = 0; i < v->count - 1; i++) {
		lval_print(v->cell[i]);
		putchar(' ');
	}
	lval_print(v->cell[i]);
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

static char *lval_to_str(struct lval *v)
{
	switch (v->type) {
	case LVAL_NUM:
		return String("%li", v->num);
	case LVAL_ERR:
		return String("\nErr: %s", v->err);
	case LVAL_SYM:
		return String("%s", v->sym);
	case LVAL_SEXPR:
		return lval_expr_to_str(v, '(', ')');
	case LVAL_QEXPR:
		return lval_expr_to_str(v, '{', '}');
	case LVAL_FUN:
		return String("<function>");
	}
	return String("Unknown lval type!");
}

static void lval_print(struct lval *v)
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
	case LVAL_SEXPR:
		lval_expr_print(v, '(', ')');
		break;
	case LVAL_QEXPR:
		lval_expr_print(v, '{', '}');
		break;
	case LVAL_FUN:
		printf("<function>");
		break;
	default:
		printf("Unknown lval type!");
		break;
	}
}

static void lval_println(struct lval *v)
{
	lval_print(v);
	putchar('\n');
}

struct lval *lval_eval_sexpr(struct lenv *e, struct lval *v)
{
	int i;

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
		return lval_err("first element is not a function!");
	}

	/* builtin */
	struct lval *result = f->fun(e, v);
	lval_free(f);
	return result;
}

struct lval *lval_join(struct lval *x, struct lval *y)
{
	while (y->count)
		x = lval_add(x, lval_pop(y, 0));
	lval_free(y);
	return x;
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

struct lval *lval_copy(struct lval *v)
{
	struct lval *x = xmalloc(sizeof(struct lval));

	x->type = v->type;

	switch (v->type) {
	/* copy direct */
	case LVAL_FUN:
		x->fun = v->fun;
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

static int lenv_get_pos(struct lenv *e, char *sym)
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
	int pos = lenv_get_pos(e, l->sym);
	return pos == -1 ? lval_err("unbound symbol '%s'", l->sym) :
				 lval_copy(e->vals[pos]);
}

static void lenv_put(struct lenv *e, struct lval *k, struct lval *v)
{
	int i = lenv_get_pos(e, k->sym);

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

static void lenv_add_builtin(struct lenv *e, char *name, lbuiltin func)
{
	struct lval *k = lval_sym(name);
	struct lval *v = lval_fun(func);
	lenv_put(e, k, v);
	lval_free(k);
	lval_free(v);
}

static void lenv_add_builtins(struct lenv *e)
{
	/* list functions */
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);
	lenv_add_builtin(e, "def", builtin_def);

	/* math functions */
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
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
		lval_free(y);
	}
	lval_free(a);
	return x;
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

struct lval *lerr_num(struct lval *a, const char *fname, char *desc,
		      int expected, int received)
{
	return lval_func_err(a, fname, "%s. Got %d, expected %d", desc,
			     received, expected);
}

struct lval *lerr_args_num(struct lval *a, const char *fname, char *desc,
			   int expected, int received)
{
	return lval_func_err(a, fname,
			     "passed %s arguments. Got %d, expected %d", desc,
			     received, expected);
}

struct lval *lerr_args_too_many(struct lval *a, const char *fname, int expected,
				int received)
{
	return lerr_args_num(a, fname, "too many", expected, received);
}

struct lval *lerr_args_too_few(struct lval *a, const char *fname, int expected,
			       int received)
{
	return lerr_args_num(a, fname, "too few", expected, received);
}
struct lval *lerr_args_type(struct lval *a, const char *fname, char *expected,
			    char *received)
{
	return lval_func_err(a, fname,
			     "passed incorrect type. Got %s, expected %s",
			     received, expected);
}

/* builtin q-expr funcs */
struct lval *builtin_head(struct lenv *e, struct lval *a)
{
	const char fname[] = "head";
	if (a->count != 1)
		return lerr_args_too_many(a, fname, a->count, 1);
	if (a->cell[0]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, ltype_name(a->cell[0]->type),
				      ltype_name(LVAL_QEXPR));
	if (a->cell[0]->count == 0)
		return lval_func_err(a, fname, "passed {}");

	struct lval *v = lval_take(a, 0);
	while (v->count > 1)
		lval_free(lval_pop(v, 1));
	return v;
}

struct lval *builtin_tail(struct lenv *e, struct lval *a)
{
	const char fname[] = "tail";
	if (a->count != 1)
		return lerr_args_too_many(a, fname, a->count, 1);
	if (a->cell[0]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, ltype_name(a->cell[0]->type),
				      ltype_name(LVAL_QEXPR));
	if (a->cell[0]->count == 0)
		return lval_func_err(a, fname, "passed {}");

	struct lval *v = lval_take(a, 0);
	lval_free(lval_pop(v, 0));
	return v;
}

struct lval *builtin_list(struct lenv *e, struct lval *a)
{
	a->type = LVAL_QEXPR;
	return a;
}

struct lval *builtin_eval(struct lenv *e, struct lval *a)
{
	const char fname[] = "eval";
	if (a->count != 1)
		return lerr_args_too_many(a, fname, a->count, 1);
	if (a->cell[0]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, ltype_name(a->cell[0]->type),
				      ltype_name(LVAL_QEXPR));

	struct lval *x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}

struct lval *builtin_join(struct lenv *e, struct lval *a)
{
	int i;
	const char fname[] = "join";
	for (i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_QEXPR)
			return lerr_args_type(a, fname,
					      ltype_name(a->cell[i]->type),
					      ltype_name(LVAL_QEXPR));
	}
	struct lval *x = lval_pop(a, 0);

	while (a->count)
		x = lval_join(x, lval_pop(a, 0));

	lval_free(a);
	return x;
}

/* def builtin */
struct lval *builtin_def(struct lenv *e, struct lval *a)
{
	int i;
	const char fname[] = "def";
	if (a->cell[0]->type != LVAL_QEXPR)
		return lerr_args_type(a, fname, ltype_name(a->cell[0]->type),
				      ltype_name(LVAL_QEXPR));

	/* first arg is symbol list */
	struct lval *syms = a->cell[0];
	for (i = 0; i < syms->count; i++)
		if (syms->cell[i]->type != LVAL_SYM)
			return lerr_args_type(a, fname, ltype_name(LVAL_SYM),
					      ltype_name(syms->cell[i]->type));

	if (syms->count != a->count - 1) {
		char *expected_syms = lval_to_str(syms);
		struct lval *tmp = lval_func_err(
			a, fname,
			"Number of symbols must match number of values.\n\n"
			"\tlen(%s) == (%d) symbol(s) given but %d values given",
			expected_syms, syms->count, a->count - 1);
		free(expected_syms);
		return tmp;
	}

	/* assign copies of values to symbols */
	for (i = 0; i < syms->count; i++)
		lenv_put(e, syms->cell[i], a->cell[i + 1]);
	lval_free(a);
	return lval_sexpr();
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
	puts(version);
	puts("Press Ctrl+c to Exit\n");

	/* Create Some Parsers */
	mpc_parser_t *Number = mpc_new("number");
	mpc_parser_t *Symbol = mpc_new("symbol");
	mpc_parser_t *Sexpr = mpc_new("sexpr");
	mpc_parser_t *Qexpr = mpc_new("qexpr");
	mpc_parser_t *Expr = mpc_new("expr");
	mpc_parser_t *Lisp = mpc_new("lisp");

	/* Define them with the following Language */
	mpca_lang(MPCA_LANG_DEFAULT,
		  "                                                   \
		number   : /-?[0-9]+/ ;                             \
		symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;       \
		sexpr     : '(' <expr>* ')' ;                       \
		qexpr     : '{' <expr>* '}' ;                       \
		expr     : <number> | <symbol> | <sexpr> | <qexpr>; \
		lisp    : /^/ <expr>* /$/ ;                         \
		",
		  Number, Symbol, Sexpr, Qexpr, Expr, Lisp);
	struct lenv *env = lenv_new();
	lenv_add_builtins(env);
	while (1) {
		mpc_result_t r;
		char *input = readline("lisp> ");
		add_history(input);
		if (mpc_parse("<stdin>", input, Lisp, &r)) {
			struct lval *x = lval_eval(env, lval_read(r.output));
			lval_println(x);
			mpc_ast_delete(r.output);
			lval_free(x);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
	}
	lenv_free(env);
	mpc_cleanup(5, Number, Symbol, Sexpr, Qexpr, Expr, Lisp);
	return 0;
}
