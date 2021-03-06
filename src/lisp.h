#ifndef _LISP_H
#define _LISP_H

#include <stdlib.h>
#include <stdio.h>
#include <execinfo.h>

#define warn(fmt, ...) fprintf(stderr, "WARN: " #fmt, __VA_ARGS__);
#define err(fmt, ...) fprintf(stderr, "ERR:" #fmt, __VA_ARGS__);

#define die(fmt, ...)                                                          \
	({                                                                     \
		err(fmt, __VA_ARGS__);                                         \
		exit(EXIT_FAILURE);                                            \
	})

#define xmalloc(size)                                                          \
	({                                                                     \
		void *_ret = malloc(size);                                     \
		if (!_ret)                                                     \
			die("%s", "failed to allocate memory\n");              \
		_ret;                                                          \
	})

#define xcalloc(size)                                                          \
	({                                                                     \
		void *_ret = calloc(1, size);                                  \
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
	LVAL_FUN_BUILTIN,
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
		long num;
		char *sym;
		char *charbuf;
		char *err;

		/* Builtin Function */
		lbuiltin builtin;

		/* Function */
		struct {
			struct lenv *env;
			struct lval *formals;
			struct lval *body;
		};

		/* Expression */
		struct {
			int count;
			struct lval **cell;
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
char *lval_to_str(struct lenv *, struct lval *);
void lval_free(struct lval *);

#endif
