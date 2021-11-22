#include <stdio.h>
#include <stdlib.h>

#include <editline/readline.h>
#include <editline/history.h>

#include <mpc.h>

static char *version = "Lisp Version 0.0.0.0.1";

static long eval_op(long x, char *op, long y)
{
	if (strcmp(op, "+") == 0)
		return x + y;
	if (strcmp(op, "-") == 0)
		return x - y;
	if (strcmp(op, "*") == 0)
		return x * y;
	if (strcmp(op, "/") == 0)
		return x / y;
	return 0;
}

static long eval(mpc_ast_t* t)
{
	/* If tagged as number return it directly. */
	if (strstr(t->tag, "number")) {
		return atoi(t->contents);
	}

	/* The operator is always second child. */
	char* op = t->children[1]->contents;

	/* We store the third child in `x` */
	long x = eval(t->children[2]);

	/* Iterate the remaining children and combining. */
	int i = 3;
	while (strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}

	return x;
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
			printf("%ld\n", eval(r.output));
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
	}
	mpc_cleanup(4, Number, Operator, Expr, Lisp);
	return 0;
}
