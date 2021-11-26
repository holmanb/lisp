#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

#include <mpc.h>

static char *version = "Lisp Version 0.0.0.0.1";

enum {
	LVAL_ERR,
	LVAL_NUM,
	LVAL_SYM,
	LVAL_SEXPR,
};

enum {
	LERR_DIV_ZERO,
	LERR_BAD_OP,
	LERR_BAD_NUM
};

struct lval {
	int type;
	long num;
	char *err;
	char *sym;
	struct {
		int count;
		struct lval **cell;
	};
};

static void lval_expr_print(struct lval *v, char open, char close);
static void lval_print(struct lval *v);
struct lval *lval_eval(struct lval *v);
struct lval *lval_take(struct lval *v, int i);
struct lval *lval_pop(struct lval *v, int i);
struct lval *builtin_op(struct lval *a, char *op);

static struct lval *lval_num(long x)
{
	struct lval *v = malloc(sizeof(struct lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

static struct lval *lval_err(char *m)
{
	struct lval *v = malloc(sizeof(struct lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(m) + 1);
	strcpy(v->err, m);
	return v;
}

static struct lval *lval_sym(char *s)
{
	struct lval *v = malloc(sizeof(struct lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

static struct lval *lval_sexpr(void)
{
	struct lval *v = malloc(sizeof(struct lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
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
		case LVAL_SEXPR:
			for (i=0; i < v-> count; i++)
				lval_free(v->cell[i]);
			free(v->cell);
			break;
		case LVAL_NUM:
			break;
	}
	free(v);
}

static struct lval *lval_read_num(mpc_ast_t *t)
{
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE ?
		lval_num(x) : lval_err("invalid number");
}

static struct lval *lval_add(struct lval *v, struct lval *x)
{
	v->count++;
	v->cell = realloc(v->cell, sizeof(struct lval *) * v->count);
	v->cell[v->count - 1] = x;
	return v;
}

static struct lval *lval_read(mpc_ast_t *t){

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

	/* put expression in list */
	for (i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0)
			continue;
		if (strcmp(t->children[i]->contents, ")") == 0)
			continue;
		if (strcmp(t->children[i]->tag, "regex") == 0)
			continue;
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}

static void lval_expr_print(struct lval *v, char open, char close)
{
	int i;
	putchar(open);

	/* print trailing space for all but last element */
	for (i = 0; i < v->count - 1; i++) {
		lval_print(v->cell[i]);
		putchar(' ');
	}
	putchar(close);
}

static void lval_print(struct lval *v)
{
	switch(v->type) {
		case LVAL_NUM:
			printf("%li", v->num);
			break;
		case LVAL_ERR:
			printf("Err: %s", v->err);
			break;
		case LVAL_SYM:
			printf("%s", v->sym);
			break;
		case LVAL_SEXPR:
			lval_expr_print(v, '(', ')');
			break;
	}
}

static void lval_println(struct lval *v) { lval_print(v); putchar('\n'); }

struct lval *lval_eval_sexpr(struct lval *v)
{
	int i;

	/* eval children*/
	for (i = 0; i < v-> count; i++) {
		v->cell[i] = lval_eval(v->cell[i]);
	}

	/* error check */
	for (i=0; i < v->count; i++){
		if(v->cell[i]->type == LVAL_ERR)
			return lval_take(v, i);
	}

	/* empty expressions */
	if(v->count == 0)
		return v;

	/* single expressions */
	if (v->count == 1)
		return lval_take(v, 0);

	/* ensure first elem is symb */
	struct lval *f = lval_pop(v, 0);
	if (f->type != LVAL_SYM){
		lval_free(f);
		lval_free(v);
		return lval_err("S-expression must start with symbol!");
	}

	/* builtin */
	struct lval *result = builtin_op(v, f->sym);
	lval_free(f);
	return result;
}


struct lval *lval_eval(struct lval *v)
{
	if (v->type == LVAL_SEXPR)
		return lval_eval_sexpr(v);
	return v;
}

struct lval *lval_pop(struct lval *v, int i)
{
	struct lval *x = v->cell[i];
	memmove(&v->cell[i], &v->cell[i+1], sizeof(struct lval *) * (v->count - i - 1));
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

struct lval *builtin_op(struct lval *a, char *op){
	int i;
	for (i=0; i<a->count; i++){
		if(a->cell[i]->type != LVAL_NUM){
			lval_free(a);
			return lval_err("Cannot operate on non-number!");

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


int main(int argc, char *argv[])
{
	puts(version);
	puts("Press Ctrl+c to Exit\n");

	/* Create Some Parsers */
	mpc_parser_t *Number = mpc_new("number");
	mpc_parser_t *Symbol = mpc_new("symbol");
	mpc_parser_t *Sexpr = mpc_new("sexpr");
	mpc_parser_t *Expr = mpc_new("expr");
	mpc_parser_t *Lisp = mpc_new("lisp");

	/* Define them with the following Language */
	mpca_lang(MPCA_LANG_DEFAULT,
		"                                                   \
		number   : /-?[0-9]+/ ;                             \
		symbol   : '+' | '-' | '*' | '/' ;                  \
		sexpr     : '(' <expr>* ')' ;                       \
		expr     : <number> | <symbol> | <sexpr> ;          \
		lisp    : /^/ <expr>* /$/ ;                         \
		",
		  Number, Symbol, Sexpr, Expr, Lisp);

	while (1) {
		mpc_result_t r;
		char *input = readline("lisp> ");
		add_history(input);
		if (mpc_parse("<stdin>", input, Lisp, &r)) {
			struct lval *x = lval_eval(lval_read(r.output));
			lval_println(x);
			mpc_ast_delete(r.output);
			lval_free(x);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
	}
	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lisp);
	return 0;
}
