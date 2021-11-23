#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

#include <mpc.h>

static char *version = "Lisp Version 0.0.0.0.1";

enum {
	LVAL_NUM,
	LVAL_ERR
};

enum {
	LERR_DIV_ZERO,
	LERR_BAD_OP,
	LERR_BAD_NUM
};

struct lval {
	int type;
	union {
		long num;
		int err;
	};
};

struct lval lval_num(long x)
{
	return (struct lval) {
		.type = LVAL_NUM,
		.num = x,
	};
}

struct lval lval_err(int x)
{
	return (struct lval) {
		.type = LVAL_ERR,
		.err = x,
	};
}

void lval_print(struct lval v){
	switch (v.type) {
		case LVAL_NUM:
			printf("%li", v.num);
			break;
		case LVAL_ERR:
			if (v.err == LERR_DIV_ZERO) {
				printf("Error: Division By Zero!");
			} else if (v.err == LERR_BAD_OP) {
				printf("Error: Invalid Operator!");
			} else if (v.err == LERR_BAD_NUM) {
				printf("Error: Invalid Number!");
			}
			break;
		default:
			printf("Unhandled error!");
			break;
	}
}

void lval_println(struct lval v) { lval_print(v); putchar('\n'); }

static struct lval eval_op(struct lval x, char *op, struct lval y)
{
	if (x.type == LVAL_ERR)
		return x;
	if (y.type == LVAL_ERR)
		return y;
	if (strcmp(op, "+") == 0)
		return lval_num(x.num + y.num);
	if (strcmp(op, "-") == 0)
		return lval_num(x.num - y.num);
	if (strcmp(op, "*") == 0)
		return lval_num(x.num * y.num);
	if (strcmp(op, "/") == 0)
		return y.num == 0
			? lval_err(LERR_DIV_ZERO)
			: lval_num(x.num / y.num);
	return lval_err(LERR_BAD_OP);
}

static struct lval eval(mpc_ast_t* t)
{
	/* If tagged as number return it directly. */
	if (strstr(t->tag, "number")) {
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
		return errno != ERANGE
			? lval_num(x)
			: lval_err(LERR_BAD_NUM);
	}

	/* The operator is always second child. */
	char* op = t->children[1]->contents;

	/* We store the third child in `x` */
	struct lval y = eval(t->children[2]);

	/* Iterate the remaining children and combining. */
	int i = 3;
	while (strstr(t->children[i]->tag, "expr")) {
		y = eval_op(y, op, eval(t->children[i]));
		i++;
	}

	return y;
}

int main(int argc, char *argv[])
{
	puts(version);
	puts("Press Ctrl+c to Exit\n");

	/* Create Some Parsers */
	mpc_parser_t *Number = mpc_new("number");
	mpc_parser_t *Operator = mpc_new("operator");
	mpc_parser_t *Expr = mpc_new("expr");
	mpc_parser_t *Lisp = mpc_new("lisp");

	/* Define them with the following Language */
	mpca_lang(MPCA_LANG_DEFAULT,
		  "                                                     \
		number   : /-?[0-9]+/ ;                             \
		operator : '+' | '-' | '*' | '/' ;                  \
		expr     : <number> | '(' <operator> <expr>+ ')' ;  \
		lisp    : /^/ <operator> <expr>+ /$/ ;             \
		",
		  Number, Operator, Expr, Lisp);

	while (1) {
		mpc_result_t r;
		char *input = readline("lisp> ");
		add_history(input);
		if (mpc_parse("<stdin>", input, Lisp, &r)) {
			struct lval result = eval(r.output);
			lval_println(result);
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
	}
	mpc_cleanup(4, Number, Operator, Expr, Lisp);
	return 0;
}
