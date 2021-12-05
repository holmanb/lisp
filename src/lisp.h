#ifndef _LISP_H
#define _LISP_H

#include <stdlib.h>
#include <stdio.h>

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

struct lenv;
struct lval;
typedef struct lval *(*lbuiltin)(struct lenv *, struct lval *);

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
		char *sym;
		char *charbuf;
		char *err;
		long num;

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


char *String(char *s, ...);
char *ltype_name(int t);

#endif
