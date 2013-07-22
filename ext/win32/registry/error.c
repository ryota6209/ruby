#include "registry.h"
#include <ruby/encoding.h>
#include <../../../internal.h>

#define wchar_encoding rb_enc_from_index(ENCINDEX_UTF_16LE)

/*
  Error
*/

static ID id_code, id_name;
static VALUE eError;

static VALUE
reg_err_initialize(VALUE obj, VALUE code, VALUE name)
{
    const DWORD flags = FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;
    union {
	WCHAR w[1024];
	char c[1];
    } msg;
    DWORD i, j, len = FormatMessageW(flags, 0, code, 0, msg.w, sizeof(msg)/sizeof(msg.w[0]), 0);
    VALUE args[1];

    for (i = 0; i < len; ++i) {
	if (msg.w[i] == L'\r') break;
    }
    for (j = i; i < len; ++i) {
	if (msg.w[i] != L'\r') msg.w[j++] = msg.w[i];
    }
    args[0] = rb_str_export_locale(rb_enc_str_new(msg.c, j * sizeof(msg.w[0]), wchar_encoding));
    rb_call_super(1, args);
    rb_ivar_set(obj, id_code, code);
    rb_ivar_set(obj, id_name, name);

    return obj;
}

void
reg_error_check(DWORD code, VALUE name)
{
    VALUE args[2];
    if (!code) return;

    args[0] = ULONG2NUM(code);
    args[1] = name;
    rb_exc_raise(rb_class_new_instance(2, args, eError));
}

VALUE
InitVM_registry_error(VALUE mod)
{
    const VALUE e = rb_define_class_under(mod, "Error", rb_eStandardError);
    eError = e;
    rb_attr(e, rb_intern("code"), TRUE, FALSE, 0);
    rb_attr(e, rb_intern("name"), TRUE, FALSE, 0);
    rb_define_method(e, "initialize", reg_err_initialize, 2);
    return e;
}

void
Init_registry_error(void)
{
    id_code = rb_intern_const("@code");
    id_name = rb_intern_const("@name");
}
