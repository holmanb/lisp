#include <stdarg.h>
#include <stdlib.h>
#include "lisp.h"
#include "lerr.h"


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

struct lval *lerr_args_num(struct lval *a, const char *fname,
				  int expected)
{
	if (expected > a->count)
		return lerr_args_too_few(a, fname, expected);
	else
		return lerr_args_too_many(a, fname, expected);
}

struct lval *lerr_args_num_desc(struct lval *a, const char *fname,
				       char *desc, int expected, int received)
{
	return lval_func_err(a, fname,
			     "passed %s arguments. Got %d, expected %d", desc,
			     received, expected);
}

struct lval *lerr_args_too_many(struct lval *a, const char *fname,
				       int expected)
{
	return lerr_args_num_desc(a, fname, "too many", expected, a->count);
}

struct lval *lerr_args_too_many_variable(struct lval *a, const char *fname,
				       int max)
{
	return lval_func_err(a, fname, "passed too many arguments. Expected"
			" no more than %d arguments", max);
}

struct lval *lerr_args_too_few(struct lval *a, const char *fname,
				      int expected)
{
	return lerr_args_num_desc(a, fname, "too few", expected, a->count);
}

struct lval *lerr_args_too_few_variable(struct lval *a, const char *fname,
				      int min)
{
	return lval_func_err(a, fname, "passed too few arguments. Expected"
			" %d or more arguments", min);
}

struct lval *lerr_args_type(struct lval *a, const char *fname,
				   int expected, int received)
{
	return lval_func_err(a, fname,
			     "passed incorrect type. Got %s, expected %s",
			     ltype_name(received), ltype_name(expected));
}

struct lval *lerr_args_mult_type(struct lval *a, const char *fname,
					int type1, int type2)
{
	return lval_func_err(
		a, fname,
		"passed multiple types. Expected one type but receive %s and %s",
		ltype_name(type1), ltype_name(type2));
}

struct lval *lerr_arg_error(struct lval *a, const char *fname, char *msg)
{
	return lval_func_err(a, fname, msg);
}
