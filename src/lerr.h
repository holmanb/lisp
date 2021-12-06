#ifndef _LERR_H
#define _LERR_H
struct lval *lerr_args_num_desc(struct lval *a, const char *fname, char *desc,
				int expected, int received);
struct lval *lerr_args_num(struct lval *lval, const char *fname, int expected);
struct lval *lerr_args_too_many(struct lval *, const char *fname, int expected);
struct lval *lerr_args_too_many_variable(struct lval *, const char *fname,
					 int max);
struct lval *lerr_args_too_few(struct lval *, const char *fname, int expected);
struct lval *lerr_args_too_few_variable(struct lval *, const char *fname,
					int min);
struct lval *lerr_args_type(struct lenv *, struct lval *, const char *fname,
			    int expected, int received);
struct lval *lerr_args_mult_type(struct lval *, const char *fname, int expected,
				 int received);
#endif
