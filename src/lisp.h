#ifndef _LISP_H
#define _LISP_H


#include <mpc.h>

struct lval;
struct lenv;

mpc_parser_t *Number;
mpc_parser_t *Symbol;
mpc_parser_t *Charbuf;
mpc_parser_t *Comment;
mpc_parser_t *Qexpr;
mpc_parser_t *Sexpr;
mpc_parser_t *Expr;
mpc_parser_t *Lisp;

typedef struct lval *(*lbuiltin)(struct lenv *, struct lval *);
static void lval_expr_print(struct lenv *, struct lval *, char open,
			    char close);
static char *lval_to_str(struct lenv *, struct lval *);
static char *lval_expr_to_str(struct lenv *, struct lval *, char open,
			      char close);

char *String(char *s, ...);
char *ltype_name(int t);
static void lval_print(struct lenv *e, struct lval *v);
static char *lenv_lookup_sym_by_val(struct lenv *e, struct lval *a);
static int lenv_get_sym_pos(struct lenv *, char *);
static struct lval *lenv_get(struct lenv *, struct lval *);
static struct lval *lval_num(long x);
static struct lval *lval_sexpr(void);
static struct lval *lval_add(struct lval *v, struct lval *x);
static struct lval *lval_eval(struct lenv *, struct lval *);
static struct lval *lval_copy(struct lval *v);
static struct lval *lval_take(struct lval *, int i);
static struct lval *lval_pop(struct lval *, int i);
static struct lval *lval_call(struct lenv *, struct lval *, struct lval *);
static struct lval *lval_str(char *s);
static struct lval *lval_err(const char *fmt, ...);

static struct lval *builtin_print(struct lenv *e, struct lval *a);
static struct lval *builtin_error(struct lenv *e, struct lval *a);
static struct lval *builtin_assert(struct lenv *, struct lval *);

static struct lval *builtin_load(struct lenv *, struct lval *);

static struct lval *builtin_type(struct lenv *, struct lval *);

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
static struct lval *builtin_mod(struct lenv *, struct lval *);

static struct lval *builtin_def(struct lenv *, struct lval *);
static struct lval *builtin_var(struct lenv *, struct lval *, char *);
static struct lval *builtin_lambda(struct lenv *, struct lval *);
static struct lval *builtin_put(struct lenv *, struct lval *);

static struct lval *builtin_order(struct lenv *, struct lval *, char *);
static struct lval *builtin_eq(struct lenv *e, struct lval *a);
static struct lval *builtin_ne(struct lenv *e, struct lval *a);
static struct lval *builtin_le(struct lenv *e, struct lval *a);
static struct lval *builtin_lt(struct lenv *e, struct lval *a);
static struct lval *builtin_ge(struct lenv *e, struct lval *a);
static struct lval *builtin_gt(struct lenv *e, struct lval *a);

static struct lval *builtin_if(struct lenv *e, struct lval *a);

static struct lval *builtin_or(struct lenv *e, struct lval *a);
static struct lval *builtin_and(struct lenv *e, struct lval *a);
static struct lval *builtin_not(struct lenv *e, struct lval *a);

static struct lval *builtin_bitwise_and(struct lenv *e, struct lval *a);
static struct lval *builtin_bitwise_or(struct lenv *e, struct lval *a);
static struct lval *builtin_bitwise_not(struct lenv *e, struct lval *a);
static struct lval *builtin_bitwise_left_shift(struct lenv *e, struct lval *a);
static struct lval *builtin_bitwise_right_shift(struct lenv *e, struct lval *a);
static struct lval *builtin_bitwise_xor(struct lenv *e, struct lval *a);

static int lval_eq(struct lval *, struct lval *);
static void lval_free(struct lval *);
struct lval *lval_func_err(struct lval *, const char *, const char *, ...);
static struct lenv *lenv_new(void);
static struct lval *lenv_get(struct lenv *, struct lval *);
static struct lenv *lenv_copy(struct lenv *e);
static void lenv_put(struct lenv *e, struct lval *k, struct lval *v);
static void lenv_free(struct lenv *e);

static struct lval *lerr_args_num_desc(struct lval *a, const char *fname,
				       char *desc, int expected, int received);
static struct lval *lerr_args_num(struct lval *lval, const char *fname,
				  int expected);
static struct lval *lerr_args_too_many(struct lval *, const char *fname,
				       int expected);
static struct lval *lerr_args_too_few(struct lval *, const char *fname,
				      int expected, int received);
static struct lval *lerr_args_type(struct lval *, const char *fname,
				   int expected, int received);
static struct lval *lerr_args_mult_type(struct lval *, const char *fname,
					int expected, int received);
static struct lval *lerr_arg_error(struct lval *a, const char *fname,
				   char *msg);
#endif
