#include "ruby.h"

extern VALUE rb_hash_delete_entry(VALUE hash, VALUE key);

static VALUE
hash_delete(VALUE hash, VALUE key)
{
    VALUE ret = rb_hash_delete_entry(hash, key);
    return UNDEF_P(ret) ? Qnil : rb_ary_new_from_values(1, &ret);
}

void
Init_delete(VALUE klass)
{
    rb_define_method(klass, "delete!", hash_delete, 1);
}
