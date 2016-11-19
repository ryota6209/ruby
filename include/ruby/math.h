#ifndef RUBY_MATH_H
#define RUBY_MATH_H 1

#include "ruby.h"

#if defined(__cplusplus)
extern "C" {
#if 0
} /* satisfy cc-mode */
#endif
#endif

RUBY_SYMBOL_EXPORT_BEGIN

/* math.c */
VALUE rb_math_frexp(VALUE);
VALUE rb_math_ldexp(VALUE, VALUE);
VALUE rb_math_exp(VALUE);
VALUE rb_math_log(int, const VALUE *);
VALUE rb_math_log2(VALUE);
VALUE rb_math_log10(VALUE);
VALUE rb_math_hypot(VALUE, VALUE);
VALUE rb_math_sin(VALUE);
VALUE rb_math_cos(VALUE);
VALUE rb_math_tan(VALUE);
VALUE rb_math_asin(VALUE);
VALUE rb_math_acos(VALUE);
VALUE rb_math_atan(VALUE);
VALUE rb_math_atan2(VALUE, VALUE);
VALUE rb_math_sinh(VALUE);
VALUE rb_math_cosh(VALUE);
VALUE rb_math_tanh(VALUE);
VALUE rb_math_asinh(VALUE);
VALUE rb_math_acosh(VALUE);
VALUE rb_math_atanh(VALUE);
VALUE rb_math_sqrt(VALUE);
VALUE rb_math_cbrt(VALUE);
VALUE rb_math_erf(VALUE);
VALUE rb_math_erfc(VALUE);
VALUE rb_math_gamma(VALUE);
VALUE rb_math_lgamma(VALUE);

/* numeric.c */
#ifndef ROUND_DEFAULT
# define ROUND_DEFAULT RUBY_NUM_ROUND_HALF_EVEN
#endif
enum ruby_num_rounding_mode {
    RUBY_NUM_ROUND_HALF_UP,
    RUBY_NUM_ROUND_HALF_EVEN,
    RUBY_NUM_ROUND_DEFAULT = ROUND_DEFAULT
};

#define RB_FIXNUM_POSITIVE_P(num) ((SIGNED_VALUE)(num) > (SIGNED_VALUE)RB_INT2FIX(0))
#define RB_FIXNUM_NEGATIVE_P(num) ((SIGNED_VALUE)(num) < 0)
#define RB_FIXNUM_ZERO_P(num) ((num) == RB_INT2FIX(0))
#define FIXNUM_POSITIVE_P(num) RB_FIXNUM_POSITIVE_P(num)
#define FIXNUM_NEGATIVE_P(num) RB_FIXNUM_NEGATIVE_P(num)
#define FIXNUM_ZERO_P(num) RB_FIXNUM_ZERO_P(num)

#define RB_INT_NEGATIVE_P(x) (RB_FIXNUM_P(x) ? RB_FIXNUM_NEGATIVE_P(x) : RBIGNUM_NEGATIVE_P(x))
#define INT_NEGATIVE_P(x) RB_INT_NEGATIVE_P(x)

int rb_num_to_uint(VALUE val, unsigned int *ret);
VALUE ruby_num_interval_step_size(VALUE from, VALUE to, VALUE step, int excl);
int ruby_float_step(VALUE from, VALUE to, VALUE step, int excl);
double ruby_float_mod(double x, double y);
int rb_num_negative_p(VALUE);
VALUE rb_int_succ(VALUE num);
VALUE rb_int_pred(VALUE num);
VALUE rb_int_uminus(VALUE num);
VALUE rb_float_uminus(VALUE num);
VALUE rb_int_plus(VALUE x, VALUE y);
VALUE rb_int_minus(VALUE x, VALUE y);
VALUE rb_int_mul(VALUE x, VALUE y);
VALUE rb_int_idiv(VALUE x, VALUE y);
VALUE rb_int_modulo(VALUE x, VALUE y);
VALUE rb_int_round(VALUE num, int ndigits, enum ruby_num_rounding_mode mode);
VALUE rb_int2str(VALUE num, int base);
VALUE rb_dbl_hash(double d);
VALUE rb_fix_plus(VALUE x, VALUE y);
VALUE rb_int_ge(VALUE x, VALUE y);
double rb_int_fdiv_double(VALUE x, VALUE y);
VALUE rb_int_pow(VALUE x, VALUE y);
VALUE rb_float_pow(VALUE x, VALUE y);
VALUE rb_int_cmp(VALUE x, VALUE y);
VALUE rb_int_equal(VALUE x, VALUE y);
VALUE rb_int_divmod(VALUE x, VALUE y);
VALUE rb_int_and(VALUE x, VALUE y);
VALUE rb_int_lshift(VALUE x, VALUE y);
VALUE rb_int_div(VALUE x, VALUE y);
VALUE rb_int_abs(VALUE num);
VALUE rb_float_abs(VALUE flt);
VALUE rb_int_positive_pow(long x, unsigned long y);
enum ruby_num_rounding_mode rb_num_get_rounding_option(VALUE opts);

/* rational.c */
VALUE rb_rational_uminus(VALUE self);
VALUE rb_rational_plus(VALUE self, VALUE other);
VALUE rb_lcm(VALUE x, VALUE y);
VALUE rb_rational_reciprocal(VALUE x);
VALUE rb_cstr_to_rat(const char *, int);
VALUE rb_rational_abs(VALUE self);

#if defined(__cplusplus)
#if 0
{ /* satisfy cc-mode */
#endif
}  /* extern "C" { */
#endif

#endif /* RUBY_MATH_H */
