#include "registry.h"
#include <ruby/encoding.h>
#include <../../../internal.h>

/*
 * = Win32 Registry
 *
 * win32/registry is registry accessor library for Win32 platform.
 * It uses importer to call Win32 Registry APIs.
 *
 * == example
 *   Win32::Registry::HKEY_CURRENT_USER.open('SOFTWARE\foo') do |reg|
 *     value = reg['foo']                               # read a value
 *     value = reg['foo', Win32::Registry::REG_SZ]      # read a value with type
 *     type, value = reg.read('foo')                    # read a value
 *     reg['foo'] = 'bar'                               # write a value
 *     reg['foo', Win32::Registry::REG_SZ] = 'bar'      # write a value with type
 *     reg.write('foo', Win32::Registry::REG_SZ, 'bar') # write a value
 *
 *     reg.each_value { |name, type, data| ... }        # Enumerate values
 *     reg.each_key { |key, wtime| ... }                # Enumerate subkeys
 *
 *     reg.delete_value(name)                         # Delete a value
 *     reg.delete_key(name)                           # Delete a subkey
 *     reg.delete_key(name, true)                     # Delete a subkey recursively
 *   end
 *
 * = Reference
 *
 * == Win32::Registry class
 *
 * --- info
 *
 * --- num_keys
 *
 * --- max_key_length
 *
 * --- num_values
 *
 * --- max_value_name_length
 *
 * --- max_value_length
 *
 * --- descriptor_length
 *
 * --- wtime
 *     Returns an item of key information.
 *
 * === constants
 * --- HKEY_CLASSES_ROOT
 *
 * --- HKEY_CURRENT_USER
 *
 * --- HKEY_LOCAL_MACHINE
 *
 * --- HKEY_PERFORMANCE_DATA
 *
 * --- HKEY_CURRENT_CONFIG
 *
 * --- HKEY_DYN_DATA
 *
 *     Win32::Registry object whose key is predefined key.
 * For detail, see the MSDN[http://msdn.microsoft.com/library/en-us/sysinfo/base/predefined_keys.asp] article.
 */

#define MAX_KEY_LENGTH 514
#define MAX_VALUE_LENGTH 32768

typedef LONG_LONG QWORD;

struct registry_key {
    HANDLE hkey;
    VALUE parent;
    VALUE keyname;
    DWORD disposition;
};

#define wchar_encoding rb_enc_from_index(ENCINDEX_UTF_16LE)

#ifdef _WIN64
# define NUM2HANDLE(n) NUM2ULL(n)
# define HANDLE2NUM(n) ULL2NUM((uintptr_t)(n))
#else
# define NUM2HANDLE(n) NUM2ULONG(n)
# define HANDLE2NUM(n) ULONG2NUM((uintptr_t)(n))
#endif

/* wtime is 100-nanosec intervals since 1601/01/01 00:00:00 UTC,
   convert it into UNIX time (since 1970/01/01 00:00:00 UTC).
   the first leap second is at 1972/06/30, so we doesn't need to think
   about it. */
#define WTIME_OFFSET ((LONG_LONG)((1970-1601)*365.2425) * 24 * 60 * 60 * 1000 * 1000 * 10)

static VALUE
wtime2time(unsigned LONG_LONG lt)
{
    lt -= WTIME_OFFSET;

    return rb_time_nano_new((time_t)(lt / (10 * 1000 * 1000)),
			    (long)(lt % (10 * 1000 * 1000)) * 100);
}

static unsigned LONG_LONG
time2wtime(VALUE time)
{
    unsigned LONG_LONG lt = NUM2ULL(rb_Integer(time));
    VALUE nsec = rb_check_funcall(time, rb_intern("nsec"), 0, 0);
    lt *= 10*1000*1000;
    if (nsec != Qundef) {
	lt += NUM2ULONG(nsec) / 100;
    }
    return lt + WTIME_OFFSET;
}

/*
 * Convert 64-bit FILETIME integer into Time object.
 */
static VALUE
reg_wtime2time(VALUE self, VALUE time)
{
    return wtime2time(NUM2ULL(time));
}

/*
 * Convert Time object or Integer object into 64-bit FILETIME.
 */
static VALUE
reg_time2wtime(VALUE self, VALUE time)
{
    unsigned LONG_LONG lt = time2wtime(time);
    return ULL2NUM(lt);
}

static VALUE
filetime_to_time(FILETIME ft)
{
    ULARGE_INTEGER tmp;

    tmp.LowPart = ft.dwLowDateTime;
    tmp.HighPart = ft.dwHighDateTime;
    return wtime2time(tmp.QuadPart);
}

static VALUE
reg_to_wstr(VALUE str)
{
    StringValueCStr(str);
    return rb_str_export_to_enc(str, wchar_encoding);
}

static void
reg_mark(void *p)
{
    struct registry_key *k = p;
    rb_gc_mark(k->parent);
    rb_gc_mark(k->keyname);
}

static void
reg_free(void *p)
{
    struct registry_key *k = p;
    if (k->hkey) {
	RegCloseKey(k->hkey);
	k->hkey = 0;
    }
    xfree(k);
}

static size_t
reg_memsize(const void *p)
{
    return p ? sizeof(struct registry_key) : 0;
}

static const rb_data_type_t reg_type = {
    "registry",
    {reg_mark, reg_free, reg_memsize,}
};

static VALUE
reg_alloc(VALUE klass)
{
    struct registry_key *k = ALLOC(struct registry_key);
    k->hkey = 0;
    k->parent = Qnil;
    k->keyname = Qnil;
    k->disposition = 0;
    return TypedData_Wrap_Struct(klass, &reg_type, k);
}

static struct registry_key *
reg_ptr(VALUE obj)
{
    struct registry_key *k = Check_TypedStruct(obj, &reg_type);
    if (!k) {
	rb_raise(rb_eArgError, "uninitialized registry key");
    }
    return k;
}

static VALUE
reg_setup(VALUE obj, HANDLE hkey, VALUE parent, VALUE keyname, DWORD disposition)
{
    struct registry_key *k = DATA_PTR(obj);
    k->hkey = hkey;
    k->parent = parent;
    k->keyname = keyname;
    k->disposition = disposition;
    return obj;
}

static VALUE
reg_new(VALUE klass, HANDLE hkey, VALUE parent, VALUE keyname, DWORD disposition)
{
    if (!NIL_P(parent)) {
	rb_check_typeddata(parent, &reg_type);
    }
    if (!NIL_P(keyname)) {
	keyname = reg_to_wstr(keyname);
    }
    return reg_setup(reg_alloc(klass), hkey, parent, keyname, disposition);
}

static const WCHAR *
make_subkey(volatile VALUE *subkeyptr)
{
    const WCHAR *ptr;
    long len;
    VALUE wsubkey, subkey = *subkeyptr;

    wsubkey = reg_to_wstr(subkey);
    ptr = (WCHAR *)RSTRING_PTR(wsubkey);
    len = RSTRING_LEN(wsubkey) / sizeof(WCHAR);

    /* chomp backslash */
    if (len > 0 && ptr[len - 1] == L'\\') {
	if (subkey == wsubkey) {
	    wsubkey = rb_str_subseq(wsubkey, 0, len * sizeof(WCHAR));
	}
	else {
	    rb_str_resize(wsubkey, --len * sizeof(WCHAR));
	}
	ptr = (WCHAR *)RSTRING_PTR(wsubkey);
    }
    *subkeyptr = wsubkey;
    return ptr;
}

static VALUE
reg_close_key(VALUE self)
{
    struct registry_key *k = reg_ptr(self);
    DWORD result = 0;
    DATA_PTR(self) = 0;
    if (k->hkey) {
	result = RegCloseKey(k->hkey);
	k->hkey = 0;
    }
    xfree(k);
    return (VALUE)result;
}

static VALUE
reg_block(VALUE key)
{
    if (rb_block_given_p()) {
	return rb_ensure(rb_yield, key, reg_close_key, key);
    }
    else {
	return key;
    }
}

static VALUE
reg_open_key(VALUE klass, VALUE keyobj, VALUE subkey, int argc, VALUE *argv)
{
    HKEY hkey = reg_ptr(keyobj)->hkey, newkey = 0;
    const WCHAR *subkeyname = make_subkey(&subkey);
    DWORD opt = (argc > 0) ? NUM2ULONG(argv[0]) : REG_OPTION_RESERVED;
    DWORD desired = (argc > 1) ? NUM2ULONG(argv[1]) : KEY_READ;

    reg_error_check(RegOpenKeyExW(hkey, subkeyname,
				  opt, desired, &newkey),
		    subkey);
    return reg_new(klass, newkey, keyobj, subkey, REG_OPENED_EXISTING_KEY);
}

static VALUE
reg_create_key(VALUE klass, VALUE keyobj, VALUE subkey, int argc, VALUE *argv)
{
    DWORD disposition;
    HKEY hkey = reg_ptr(keyobj)->hkey, newkey = 0;
    const WCHAR *subkeyname = make_subkey(&subkey);
    DWORD opt = (argc > 0) ? NUM2ULONG(argv[0]) : REG_OPTION_RESERVED;
    DWORD desired = (argc > 1) ? NUM2ULONG(argv[1]) : KEY_ALL_ACCESS;

    reg_error_check(RegCreateKeyExW(hkey, subkeyname, 0, 0,
				    opt, desired, 0, &newkey, &disposition),
		    subkey);
    return reg_new(klass, newkey, keyobj, subkey, disposition);
}

/*
 * --- Registry.open(key, subkey, desired = KEY_READ, opt = REG_OPTION_RESERVED)
 *
 * --- Registry.open(key, subkey, desired = KEY_READ, opt = REG_OPTION_RESERVED) { |reg| ... }
 *
 * Open the registry key subkey under key.
 * key is Win32::Registry object of parent key.
 * You can use predefined key HKEY_* (see Constants)
 * desired and opt is access mask and key option.
 * For detail, see the MSDN[http://msdn.microsoft.com/library/en-us/sysinfo/base/regopenkeyex.asp].
 * If block is given, the key is closed automatically.
 */
static VALUE
reg_s_open(int argc, VALUE *argv, VALUE klass)
{
    rb_check_arity(argc, 2, 4);
    return reg_block(reg_open_key(klass, argv[0], argv[1], argc-2, argv+2));
}

/*
 * --- Registry.create(key, subkey, desired = KEY_ALL_ACCESS, opt = REG_OPTION_RESERVED)
 *
 * --- Registry.create(key, subkey, desired = KEY_ALL_ACCESS, opt = REG_OPTION_RESERVED) { |reg| ... }
 *
 * Create or open the registry key subkey under key.
 * You can use predefined key HKEY_* (see Constants)
 *
 * If subkey is already exists, key is opened and Registry#created?
 * method will return false.
 *
 * If block is given, the key is closed automatically.
 */
static VALUE
reg_s_create(int argc, VALUE *argv, VALUE klass)
{
    rb_check_arity(argc, 2, 4);
    return reg_block(reg_create_key(klass, argv[0], argv[1], argc-2, argv+2));
}

#define ATTR_READER(n, conv) \
static VALUE reg_##n(VALUE self) {return conv(reg_ptr(self)->n);}

/*
 * Returns key handle value.
 */
ATTR_READER(hkey, HANDLE2NUM)

/*
 * Win32::Registry object of parent key, or nil if predefeined key.
 */
ATTR_READER(parent, +)

/*
 * Same as subkey value of Registry.open or
 * Registry.create method.
 */
ATTR_READER(keyname, +)

/*
 * Disposition value (REG_CREATED_NEW_KEY or REG_OPENED_EXISTING_KEY).
 */
ATTR_READER(disposition, ULONG2NUM)

/*
 * Returns if key is created ((*newly*)).
 * (see Registry.create) -- basically you call create
 * then when you call created? on the instance returned
 * it will tell if it was successful or not
 */
static VALUE
reg_created_p(VALUE self)
{
    return reg_ptr(self)->disposition == REG_CREATED_NEW_KEY ? Qtrue : Qfalse;
}

/*
 * Returns if key is not closed.
 */
static VALUE
reg_open_p(VALUE self)
{
    return reg_ptr(self)->hkey ? Qtrue : Qfalse;
}

/*
 * Full path of key such as 'HKEY_CURRENT_USER\SOFTWARE\foo\bar'.
 */
static VALUE
reg_name(VALUE self)
{
    struct registry_key *k = Check_TypedStruct(self, &reg_type);
    VALUE names;

    if (!k) return Qnil;
    names = rb_ary_new();

    while (rb_ary_push(names, k->keyname), !NIL_P(self = k->parent)) {
	k = reg_ptr(self);
    }
    names = rb_ary_join(names, rb_enc_str_new("\\", 2, wchar_encoding));
    RB_GC_GUARD(self);
    return rb_str_export_to_enc(names, rb_locale_encoding());
}

static VALUE
reg_inspect(VALUE self)
{
    VALUE name = reg_name(self);
    if (NIL_P(name)) {
	return rb_sprintf("#<%"PRIsVALUE" uninitialized", rb_obj_class(self));
    }
    else {
	return rb_sprintf("#<%"PRIsVALUE" key=%+"PRIsVALUE, rb_obj_class(self), name);
    }
}

/*
 * Same as Win32::Registry.open (self, subkey, desired, opt)
 */
static VALUE
reg_open(int argc, VALUE *argv, VALUE self)
{
    rb_check_arity(argc, 1, 3);
    return reg_block(reg_open_key(rb_obj_class(self), self, argv[0], argc-2, argv+2));
}

/*
 * Same as Win32::Registry.create (self, subkey, desired, opt)
 */
static VALUE
reg_create(int argc, VALUE *argv, VALUE self)
{
    rb_check_arity(argc, 1, 3);
    return reg_block(reg_create_key(rb_obj_class(self), self, argv[0], argc-2, argv+2));
}

/*
 * Close key.
 *
 * After close, most method raise an error.
 */
static VALUE
reg_close(VALUE self)
{
    reg_error_check((DWORD)reg_close_key(self), Qnil);
    return self;
}

static DWORD
reg_query_value(HKEY hkey, const WCHAR *name, VALUE *data)
{
    DWORD type = 0, size = 0;
    VALUE d;
    reg_error_check(RegQueryValueExW(hkey, name, 0, &type, 0, &size), Qnil);
    d = rb_str_buf_new(size);
    reg_error_check(RegQueryValueExW(hkey, name, 0, &type, (BYTE *)RSTRING_PTR(d), &size),
		    Qnil);
    *data = d;
    return type;
}

static VALUE
reg_decode_value(VALUE name, DWORD type, VALUE data, int expand_environ)
{
    const char *ptr;
    long len;

    RSTRING_GETMEM(data, ptr, len);
    switch (type) {
      case REG_SZ:
	len /= sizeof(WCHAR);
	if (len > 0 && !((const WCHAR *)ptr)[len - 1]) {
	    --len;
	    rb_str_set_len(data, len * sizeof(WCHAR));
	}
	return rb_str_conv_enc(data, wchar_encoding, rb_enc_get(name));
      case REG_EXPAND_SZ:
	{
	    DWORD size = ExpandEnvironmentStringsW((const WCHAR *)ptr, NULL, 0);
	    VALUE expanded_string = rb_str_tmp_new(size * sizeof(WCHAR) - 1);
	    WCHAR *dest = (WCHAR *)RSTRING_PTR(expanded_string);
	    ExpandEnvironmentStringsW((const WCHAR *)ptr, dest, size);
	    rb_str_set_len(expanded_string, lstrlenW(dest) * sizeof(WCHAR));
	    rb_str_replace(data, expanded_string);
	}
	return rb_str_conv_enc(data, wchar_encoding, rb_enc_get(name));
      case REG_MULTI_SZ:
	{
	    VALUE result = rb_ary_new();
	    rb_encoding *enc = rb_enc_get(name);
	    const char *end;
	    data = rb_str_conv_enc(data, wchar_encoding, enc);
	    RSTRING_GETMEM(data, ptr, len);
	    end = ptr + len;
	    while (ptr < end) {
		const char *next = memchr(ptr, '\0', len);
		size_t n = next ? next - ptr : len;
		rb_ary_push(result, rb_enc_str_new(ptr, n, enc));
		if (!next) break;
		ptr = next + 1;
	    }
	    return result;
	}
      case REG_BINARY:
        return data;
      case REG_DWORD:
	if (len == sizeof(DWORD)) {
	    DWORD dw = *(DWORD *)ptr;
	    return ULONG2NUM(dw);
	}
	break;
      case REG_DWORD_BIG_ENDIAN:
	if (len == sizeof(DWORD)) {
	    DWORD dw = ((unsigned char)ptr[0] << 24) |
		((unsigned char)ptr[1] << 16) |
		((unsigned char)ptr[2] << 8) |
		(unsigned char)ptr[3];
	    return ULONG2NUM(dw);
	}
	break;
      case REG_QWORD:
	if (len == sizeof(QWORD)) {
	    QWORD qw = *(QWORD *)ptr;
	    return ULL2NUM(qw);
	}
	break;
    }
    return Qundef;
}

/*
 * Enumerate values.
 */
static VALUE
reg_each_value(VALUE self)
{
    HKEY hkey = reg_ptr(self)->hkey;
    WCHAR name[MAX_KEY_LENGTH];
    DWORD size, index;
#define EnumValue(hkey, index, name, size) \
    (size = MAX_KEY_LENGTH, RegEnumValueW(hkey, index, name, &size, 0, 0, 0, 0))

    for (index = 0; EnumValue(hkey, index, name, size) == ERROR_SUCCESS; ++index) {
	VALUE data, subkey = rb_locale_str_new((char *)name, size * sizeof(name[0]));
	DWORD type = reg_query_value(hkey, name, &data);
	data = reg_decode_value(subkey, type, data, FALSE);
	if (data != Qundef) {
	    rb_yield_values(3, subkey, ULONG2NUM(type), data);
	}
    }
    RB_GC_GUARD(self);
    return ULONG2NUM(index);
}

#define EnumKey(hkey, index, name, size, wtime)				\
    (size = MAX_KEY_LENGTH, RegEnumKeyExW(hkey, index, name, &size, 0, 0, 0, wtime))

/*
 * Enumerate subkeys.
 *
 * subkey is String which contains name of subkey.
 * wtime is last write time as FILETIME (64-bit integer).
 * (see Registry.wtime2time)
 */
static VALUE
reg_each_key(VALUE self)
{
    HKEY hkey = reg_ptr(self)->hkey;
    WCHAR name[MAX_KEY_LENGTH];
    DWORD size, index;
    FILETIME wtime;

    for (index = 0; EnumKey(hkey, index, name, size, &wtime) == ERROR_SUCCESS; ++index) {
	VALUE subkey = rb_locale_str_new((char *)name, size * sizeof(name[0]));
	rb_yield_values(2, subkey, filetime_to_time(wtime));
    }
    RB_GC_GUARD(self);
    return ULONG2NUM(index);
}

/*
 * return keys as an array
 */
static VALUE
reg_keys(VALUE self)
{
    VALUE keys = rb_ary_new();
    HKEY hkey = reg_ptr(self)->hkey;
    WCHAR name[MAX_KEY_LENGTH];
    DWORD size, index;

    for (index = 0; EnumKey(hkey, index, name, size, NULL) == ERROR_SUCCESS; ++index) {
	VALUE subkey = rb_locale_str_new((char *)name, size * sizeof(name[0]));
	rb_ary_push(keys, subkey);
    }
    RB_GC_GUARD(self);
    return keys;
}

static DWORD
reg_read_data(VALUE self, VALUE name, int argc, const VALUE *argv, VALUE *data, int expand_environ)
{
    DWORD type;
    VALUE wname = reg_to_wstr(name);
    type = reg_query_value(reg_ptr(self)->hkey, (const WCHAR *)RSTRING_PTR(wname), data);
    if (argc > 0) {
	VALUE rtype = ULONG2NUM(type);
	int i = 0;
	while (argv[i] != rtype) {
	    if (++i >= argc) {
		VALUE expected = rb_ary_new_from_values(argc, argv);
		rb_raise(rb_eTypeError, "Type mismatch (expect %+"PRIsVALUE" but %lu present)",
			 expected, type);
	    }
	}
    }
    if ((*data = reg_decode_value(name, type, *data, expand_environ)) == Qundef) {
        rb_raise(rb_eTypeError, "Type %lu is not supported.", type);
    }
    return type;
}

/*
 * Read a registry value named name and return array of
 * [ type, data ].
 * When name is nil, the `default' value is read.
 * type is value type. (see Win32::Registry::Constants module)
 * data is value data, its class is:
 * :REG_SZ, REG_EXPAND_SZ
 *    String
 * :REG_MULTI_SZ
 *    Array of String
 * :REG_DWORD, REG_DWORD_BIG_ENDIAN, REG_QWORD
 *    Integer
 * :REG_BINARY
 *    String (contains binary data)
 *
 * When rtype is specified, the value type must be included by
 * rtype array, or TypeError is raised.
 */
static VALUE
reg_read(int argc, VALUE *argv, VALUE self)
{
    VALUE data;
    DWORD type;

    rb_check_arity(argc, 1, UNLIMITED_ARGUMENTS);
    type = reg_read_data(self, argv[0], argc-1, argv+1, &data, FALSE);
    return rb_assoc_new(ULONG2NUM(type), data);
}

/*
 * Read a registry value named name and return its value data.
 * The class of value is same as #read method returns.
 *
 * If the value type is REG_EXPAND_SZ, returns value data whose environment
 * variables are replaced.
 * If the value type is neither REG_SZ, REG_MULTI_SZ, REG_DWORD,
 * REG_DWORD_BIG_ENDIAN, nor REG_QWORD, TypeError is raised.
 *
 * The meaning of rtype is same as #read method.
 */
static VALUE
reg_aref(int argc, VALUE *argv, VALUE self)
{
    VALUE  data;

    rb_check_arity(argc, 1, UNLIMITED_ARGUMENTS);
    reg_read_data(self, argv[0], argc-1, argv+1, &data, TRUE);
    return data;
}

/*
 * Read a REG_SZ(read_s), REG_DWORD(read_i), or REG_BINARY(read_bin)
 * registry value named name.
 *
 * If the values type does not match, TypeError is raised.
 */
static VALUE
reg_read_s(VALUE self, VALUE name)
{
    static const VALUE rtypes[] = {INT2FIX(REG_SZ)};
    VALUE data;
    reg_read_data(self, name, numberof(rtypes), rtypes, &data, FALSE);
    return data;
}

/*
 * Read a REG_SZ or REG_EXPAND_SZ registry value named name.
 *
 * If the value type is REG_EXPAND_SZ, environment variables are replaced.
 * Unless the value type is REG_SZ or REG_EXPAND_SZ, TypeError is raised.
 */
static VALUE
reg_read_s_expand(VALUE self, VALUE name)
{
    static const VALUE rtypes[] = {INT2FIX(REG_SZ), INT2FIX(REG_EXPAND_SZ)};
    VALUE data;
    reg_read_data(self, name, numberof(rtypes), rtypes, &data, TRUE);
    return data;
}

/*
 * Read a REG_SZ(read_s), REG_DWORD(read_i), or REG_BINARY(read_bin)
 * registry value named name.
 *
 * If the values type does not match, TypeError is raised.
 */
static VALUE
reg_read_i(VALUE self, VALUE name)
{
    static const VALUE rtypes[] = {
	INT2FIX(REG_DWORD), INT2FIX(REG_DWORD_BIG_ENDIAN), INT2FIX(REG_QWORD),
    };
    VALUE data;
    reg_read_data(self, name, numberof(rtypes), rtypes, &data, FALSE);
    return data;
}

/*
 * Read a REG_SZ(read_s), REG_DWORD(read_i), or REG_BINARY(read_bin)
 * registry value named name.
 *
 * If the values type does not match, TypeError is raised.
 */
static VALUE
reg_read_bin(VALUE self, VALUE name)
{
    static const VALUE rtypes[] = {INT2FIX(REG_BINARY)};
    VALUE data;
    reg_read_data(self, name, numberof(rtypes), rtypes, &data, FALSE);
    return data;
}

static VALUE
reg_string_to_write(VALUE data)
{
    return reg_to_wstr(data);
}

static VALUE
reg_array_to_write(VALUE data)
{
    VALUE zero = rb_str_new("", 1);
    data = rb_ary_join(rb_Array(data), zero);
    rb_str_append(data, zero);
    return reg_to_wstr(data);
}

static void
reg_set_value(HKEY hkey, VALUE name, DWORD type, const void *ptr, DWORD size)
{
    name = reg_to_wstr(name);
    reg_error_check(RegSetValueExW(hkey, (const WCHAR *)RSTRING_PTR(name), 0UL, type, ptr, size),
		    name);
}

static void
reg_write_data(VALUE self, VALUE name, DWORD type, VALUE data)
{
    const void *ptr;
    DWORD size;
    HKEY hkey = reg_ptr(self)->hkey;
    DWORD dw;
    QWORD qw;

    switch (type) {
      case REG_SZ:
      case REG_EXPAND_SZ:
	StringValue(data);
	data = reg_string_to_write(data);
	RSTRING_GETMEM(data, ptr, size);
	size += sizeof(WCHAR);
	break;
      case REG_MULTI_SZ:
	data = reg_array_to_write(data);
	RSTRING_GETMEM(data, ptr, size);
	break;
      case REG_BINARY:
	StringValue(data);
	RSTRING_GETMEM(data, ptr, size);
	break;
      case REG_DWORD:
	dw = NUM2ULONG(data);
	ptr = &dw;
	size = sizeof(dw);
	break;
      case REG_DWORD_BIG_ENDIAN:
	dw = NUM2ULONG(data);
	dw = ((dw >> 24) & 0xff) |
	    ((dw >> 8) & 0xff00) |
	    ((dw << 8) & 0xff0000) |
	    ((dw << 24) & 0xff000000);
	ptr = &dw;
	size = sizeof(dw);
	break;
      case REG_QWORD:
	qw = NUM2ULL(data);
	ptr = &qw;
	size = sizeof(qw);
	break;
      default:
        rb_raise(rb_eTypeError, "Unsupported type %lu", type);
    }
    reg_set_value(hkey, name, type, ptr, size);
    RB_GC_GUARD(data);
}

/*
 * Write data to a registry value named name.
 * When name is nil, write to the `default' value.
 *
 * type is type value. (see Registry::Constants module)
 * Class of data must be same as which #read
 * method returns.
 */
static VALUE
reg_write(VALUE self, VALUE name, VALUE type, VALUE data)
{
    reg_write_data(self, name, NUM2ULONG(type), data);
    return Qnil;
}

/*
 * Write value to a registry value named name.
 *
 * If wtype is specified, the value type is it.
 * Otherwise, the value type is depend on class of value:
 * :Integer
 *   REG_DWORD
 * :String
 *   REG_SZ
 * :Array
 *   REG_MULTI_SZ
 */
static VALUE
reg_aset(int argc, VALUE *argv, VALUE self)
{
    VALUE data, name, value = Qnil;
    const char *ptr;
    DWORD size;

    rb_check_arity(argc, 2, 3);
    if (argc == 3) {
	reg_write(self, argv[0], argv[1], argv[2]);
    }
    else {
	HKEY hkey = reg_ptr(self)->hkey;
	name = argv[0];
	value = argv[1];
	if (!NIL_P(data = rb_check_to_int(value))) {
	    DWORD dw = NUM2ULONG(data);
	    reg_set_value(hkey, name, REG_DWORD, &dw, sizeof(dw));
	}
	else if (!NIL_P(data = rb_check_string_type(value))) {
	    data = reg_string_to_write(data);
	    RSTRING_GETMEM(data, ptr, size);
	    size += sizeof(WCHAR);
	    reg_set_value(hkey, name, REG_SZ, ptr, size);
	}
	else if (!NIL_P(data = rb_check_array_type(value))) {
	    data = reg_array_to_write(data);
	    RSTRING_GETMEM(data, ptr, size);
	    reg_set_value(hkey, name, REG_MULTI_SZ, ptr, size + sizeof(WCHAR));
	}
	else {
	    rb_raise(rb_eTypeError, "Unexpected type %"PRIsVALUE, rb_obj_class(value));
	}
    }
    return value;
}


/*
 * Write value to a registry value named name.
 *
 * The value type is REG_SZ(write_s), REG_DWORD(write_i), or
 * REG_BINARY(write_bin).
 */
static VALUE
reg_write_s(VALUE self, VALUE name, VALUE value)
{
    const void *ptr;
    DWORD size;
    value = reg_string_to_write(rb_obj_as_string(value));
    RSTRING_GETMEM(value, ptr, size);
    size += sizeof(WCHAR);
    reg_set_value(reg_ptr(self)->hkey, name, REG_SZ, ptr, size);
    return Qnil;
}

/*
 * Write value to a registry value named name.
 *
 * The value type is REG_SZ(write_s), REG_DWORD(write_i), or
 * REG_BINARY(write_bin).
 */
static VALUE
reg_write_i(VALUE self, VALUE name, VALUE value)
{
    DWORD dw = NUM2ULONG(rb_Integer(value));
    reg_set_value(reg_ptr(self)->hkey, name, REG_DWORD, &dw, sizeof(dw)), name;
    return Qnil;
}

/*
 * Write value to a registry value named name.
 *
 * The value type is REG_SZ(write_s), REG_DWORD(write_i), or
 * REG_BINARY(write_bin).
 */
static VALUE
reg_write_bin(VALUE self, VALUE name, VALUE value)
{
    const void *ptr;
    DWORD size;
    value = rb_obj_as_string(value);
    RSTRING_GETMEM(value, ptr, size);
    reg_set_value(reg_ptr(self)->hkey, name, REG_BINARY, ptr, size), name;
    return Qnil;
}

/*
 * Delete a registry value named name.
 * We can not delete the `default' value.
 */
static VALUE
reg_delete_value(VALUE self, VALUE name)
{
    name = reg_to_wstr(name);
    reg_error_check(RegDeleteValueW(reg_ptr(self)->hkey, (const WCHAR *)RSTRING_PTR(name)), name);
    return Qnil;
}

static void
reg_delete_key_recursive(HKEY hkey, const WCHAR *keyname)
{
    HKEY subkey;
    WCHAR name[MAX_KEY_LENGTH];
    DWORD size, index;
    const DWORD opt = REG_OPTION_RESERVED;
    const DWORD desired = KEY_ALL_ACCESS;

    if (!RegOpenKeyExW(hkey, keyname, opt, desired, &subkey)) {
	for (index = 0; EnumKey(subkey, index, name, size, NULL) == ERROR_SUCCESS; ++index) {
	    reg_delete_key_recursive(subkey, name);
	}
	RegCloseKey(subkey);
    }
}

/*
 * Delete a subkey named name and all its values.
 *
 * If recursive is false, the subkey must not have subkeys.
 * Otherwise, this method deletes all subkeys and values recursively.
 */
static VALUE
reg_delete_key(int argc, VALUE *argv, VALUE self)
{
    int recursive = FALSE;
    HKEY hkey;
    VALUE name;
    const WCHAR *wname;

    rb_check_arity(argc, 1, 2);
    hkey = reg_ptr(self)->hkey;
    name = reg_to_wstr(argv[0]);
    wname = (const WCHAR *)RSTRING_PTR(name);
    if (argc) recursive = RTEST(argv[0]);
    if (recursive) {
	reg_delete_key_recursive(hkey, wname);
    }
    reg_error_check(RegDeleteKeyW(hkey, wname), name);
    return Qnil;
}

/*
 * Write all the attributes into the registry file.
 */
static VALUE
reg_flush(VALUE self)
{
    reg_error_check(RegFlushKey(reg_ptr(self)->hkey), Qnil);
    return Qnil;
}

struct reg_key_info {
    DWORD num_keys;
    DWORD max_key_length;
    DWORD num_values;
    DWORD max_value_name_length;
    DWORD max_value_length;
    DWORD descriptor_length;
    FILETIME wtime;
};

static struct reg_key_info *
reg_query_info(VALUE self, struct reg_key_info *info)
{
    DWORD ret = RegQueryInfoKey(reg_ptr(self)->hkey,
				0, 0, 0,
				&info->num_keys,
				&info->max_key_length, 0,
				&info->num_values,
				&info->max_value_name_length,
				&info->max_value_length,
				&info->descriptor_length,
				&info->wtime);
    reg_error_check(ret, Qnil);
    return info;
}

/*
 * Returns key information as Array of:
 * :num_keys
 *   The number of subkeys.
 * :max_key_length
 *   Maximum length of name of subkeys.
 * :num_values
 *   The number of values.
 * :max_value_name_length
 *   Maximum length of name of values.
 * :max_value_length
 *   Maximum length of value of values.
 * :descriptor_length
 *   Length of security descriptor.
 * :wtime
 *   Last write time as FILETIME(64-bit integer)
 *
 * For detail, see RegQueryInfoKey[http://msdn.microsoft.com/library/en-us/sysinfo/base/regqueryinfokey.asp] Win32 API.
 */
static VALUE
reg_info(VALUE self)
{
    struct reg_key_info info;
    reg_query_info(self, &info);

    return rb_ary_new_from_args(7,
				ULONG2NUM(info.num_keys),
				ULONG2NUM(info.max_key_length),
				ULONG2NUM(info.num_values),
				ULONG2NUM(info.max_value_name_length),
				ULONG2NUM(info.max_value_length),
				ULONG2NUM(info.descriptor_length),
				filetime_to_time(info.wtime));
}

#define DEFINE_INFO_ACCESSOR(func, attr) \
static VALUE			 \
reg_##attr(VALUE self)		 \
{				 \
    struct reg_key_info info;	 \
    reg_query_info(self, &info); \
    return func(info.attr);	 \
}

DEFINE_INFO_ACCESSOR(ULONG2NUM, num_keys)
DEFINE_INFO_ACCESSOR(ULONG2NUM, max_key_length)
DEFINE_INFO_ACCESSOR(ULONG2NUM, num_values)
DEFINE_INFO_ACCESSOR(ULONG2NUM, max_value_name_length)
DEFINE_INFO_ACCESSOR(ULONG2NUM, max_value_length)
DEFINE_INFO_ACCESSOR(ULONG2NUM, descriptor_length)
DEFINE_INFO_ACCESSOR(filetime_to_time, wtime)

static VALUE
predefine_key(VALUE klass, HANDLE hkey, ID name)
{
    VALUE obj = reg_new(klass, hkey, Qnil, rb_id2str(name), REG_OPENED_EXISTING_KEY);
    rb_const_set(klass, name, obj);
    return obj;
}

void
InitVM_registry(void)
{
    VALUE mWin32 = rb_define_module("Win32");
    VALUE cRegistry = rb_define_class_under(mWin32, "Registry", rb_cObject);

    rb_undef_alloc_func(cRegistry);

    rb_include_module(cRegistry, InitVM_registry_constants(cRegistry));
    rb_include_module(cRegistry, rb_mEnumerable);
    InitVM_predefined_key(cRegistry, predefine_key);
    InitVM_registry_error(cRegistry);

    rb_define_singleton_method(cRegistry, "wtime2time", reg_wtime2time, 1);
    rb_define_singleton_method(cRegistry, "time2wtime", reg_time2wtime, 1);
    rb_define_singleton_method(cRegistry, "open", reg_s_open, -1);
    rb_define_singleton_method(cRegistry, "create", reg_s_create, -1);

#define DECL_ATTR_READER(name) rb_define_method(cRegistry, #name, reg_##name, 0)
    DECL_ATTR_READER(hkey);
    DECL_ATTR_READER(parent);
    DECL_ATTR_READER(keyname);
    DECL_ATTR_READER(disposition);
    rb_define_method(cRegistry, "created?", reg_created_p, 0);
    rb_define_method(cRegistry, "open?", reg_open_p, 0);
    DECL_ATTR_READER(name);
    DECL_ATTR_READER(inspect);
    rb_define_method(cRegistry, "open", reg_open, -1);
    rb_define_method(cRegistry, "create", reg_create, -1);
    rb_define_method(cRegistry, "close", reg_close, 0);
    rb_define_method(cRegistry, "each_value", reg_each_value, 0);
    rb_define_method(cRegistry, "each", reg_each_value, 0);
    rb_define_method(cRegistry, "each_key", reg_each_key, 0);
    rb_define_method(cRegistry, "keys", reg_keys, 0);
    rb_define_method(cRegistry, "read", reg_read, -1);
    rb_define_method(cRegistry, "[]", reg_aref, -1);
    rb_define_method(cRegistry, "read_s", reg_read_s, 1);
    rb_define_method(cRegistry, "read_s_expand", reg_read_s_expand, 1);
    rb_define_method(cRegistry, "read_i", reg_read_i, 1);
    rb_define_method(cRegistry, "read_bin", reg_read_bin, 1);
    rb_define_method(cRegistry, "write", reg_write, 3);
    rb_define_method(cRegistry, "[]=", reg_aset, -1);
    rb_define_method(cRegistry, "write_s", reg_write_s, 2);
    rb_define_method(cRegistry, "write_i", reg_write_i, 2);
    rb_define_method(cRegistry, "write_bin", reg_write_bin, 2);
    rb_define_method(cRegistry, "delete_value", reg_delete_value, 1);
    rb_define_method(cRegistry, "delete", reg_delete_value, 1);
    rb_define_method(cRegistry, "delete_key", reg_delete_key, -1);
    rb_define_method(cRegistry, "flush", reg_flush, 0);
    rb_define_method(cRegistry, "info", reg_info, 0);
    rb_define_method(cRegistry, "num_keys", reg_num_keys, 0);
    rb_define_method(cRegistry, "max_key_length", reg_max_key_length, 0);
    rb_define_method(cRegistry, "num_values", reg_num_values, 0);
    rb_define_method(cRegistry, "max_value_name_length", reg_max_value_name_length, 0);
    rb_define_method(cRegistry, "max_value_length", reg_max_value_length, 0);
    rb_define_method(cRegistry, "descriptor_length", reg_descriptor_length, 0);
    rb_define_method(cRegistry, "wtime", reg_wtime, 0);
}

void
Init_registry(void)
{
    Init_registry_constants();
    Init_predefined_key();
    Init_registry_error();
    InitVM(registry);
}
