#include "ruby.h"

static VALUE
bug_sym_check_id(VALUE self, VALUE name)
{
    ID id = rb_check_id(&name);
    return id ? ID2SYM(id) : name;
}

#ifdef HAVE_RB_TO_SYM
ID rb_to_sym(VALUE);
static VALUE
bug_sym_to_sym(VALUE self, VALUE name)
{
    return rb_to_sym(name);
}
#endif

void
Init_intern(VALUE klass)
{
    rb_define_singleton_method(klass, "check_id", bug_sym_check_id, 1);
#ifdef HAVE_RB_TO_SYM
    rb_define_singleton_method(klass, "to_sym", bug_sym_to_sym, 1);
#endif
}
