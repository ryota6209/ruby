#include "registry.h"

/*
For detail, see the MSDN[http://msdn.microsoft.com/library/en-us/sysinfo/base/registry.asp].

--- HKEY_*

    Predefined key ((*handle*)).
    These are Integer, not Win32::Registry.

--- REG_*

    Registry value type.

--- KEY_*

    Security access mask.

--- KEY_OPTIONS_*

    Key options.

--- REG_CREATED_NEW_KEY

--- REG_OPENED_EXISTING_KEY

    If the key is created newly or opened existing key.
    See also Registry#disposition method.
*/

static ID id_c_type2name;

static VALUE
reg_s_type2name(VALUE self, VALUE type)
{
    VALUE name = rb_hash_lookup(rb_cvar_get(self, id_c_type2name), type);
    if (NIL_P(name)) {
	name = rb_obj_as_string(type);
    }
    return name;
}

VALUE
InitVM_registry_constants(VALUE mod)
{
    const VALUE c = rb_define_module_under(mod, "Constants");
    const VALUE type2name = rb_hash_new();

#define D(n) rb_define_const(c, #n, INT2FIX(n))
    D(REG_NONE);
    D(REG_SZ);
    D(REG_EXPAND_SZ);
    D(REG_BINARY);
    D(REG_DWORD);
    D(REG_DWORD_LITTLE_ENDIAN);
    D(REG_DWORD_BIG_ENDIAN);
    D(REG_LINK);
    D(REG_MULTI_SZ);
    D(REG_RESOURCE_LIST);
    D(REG_FULL_RESOURCE_DESCRIPTOR);
    D(REG_RESOURCE_REQUIREMENTS_LIST);
    D(REG_QWORD);
    D(REG_QWORD_LITTLE_ENDIAN);

    D(STANDARD_RIGHTS_READ);
    D(STANDARD_RIGHTS_WRITE);
    D(KEY_QUERY_VALUE);
    D(KEY_SET_VALUE);
    D(KEY_CREATE_SUB_KEY);
    D(KEY_ENUMERATE_SUB_KEYS);
    D(KEY_NOTIFY);
    D(KEY_CREATE_LINK);
    D(KEY_READ);
    D(KEY_WRITE);
    D(KEY_EXECUTE);
    D(KEY_ALL_ACCESS);

    D(REG_OPTION_RESERVED);
    D(REG_OPTION_NON_VOLATILE);
    D(REG_OPTION_VOLATILE);
    D(REG_OPTION_CREATE_LINK);
    D(REG_OPTION_BACKUP_RESTORE);
    D(REG_OPTION_OPEN_LINK);
    D(REG_LEGAL_OPTION);

    D(REG_CREATED_NEW_KEY);
    D(REG_OPENED_EXISTING_KEY);

    D(REG_WHOLE_HIVE_VOLATILE);
    D(REG_REFRESH_HIVE);
    D(REG_NO_LAZY_FLUSH);
    D(REG_FORCE_RESTORE);
#undef D

#define D(n) rb_hash_aset(type2name, INT2FIX(n), rb_str_new_cstr(#n))
    D(REG_NONE);
    D(REG_SZ);
    D(REG_EXPAND_SZ);
    D(REG_BINARY);
    D(REG_DWORD);
    D(REG_DWORD_BIG_ENDIAN);
    D(REG_LINK);
    D(REG_MULTI_SZ);
    D(REG_RESOURCE_LIST);
    D(REG_FULL_RESOURCE_DESCRIPTOR);
    D(REG_RESOURCE_REQUIREMENTS_LIST);
    D(REG_QWORD);
#undef D

    rb_cvar_set(mod, id_c_type2name, type2name);
    rb_define_singleton_method(mod, "type2name", reg_s_type2name, 1);

    return c;
}

void
Init_registry_constants(void)
{
    id_c_type2name = rb_intern_const("@@type2name");
}

VALUE
InitVM_predefined_key(VALUE mod, predefine_key_func *predefine_key)
{
#define D(n) predefine_key(mod, n, rb_intern(#n))
    D(HKEY_CLASSES_ROOT);
    D(HKEY_CURRENT_USER);
    D(HKEY_LOCAL_MACHINE);
    D(HKEY_USERS);
    D(HKEY_PERFORMANCE_DATA);
    D(HKEY_PERFORMANCE_TEXT);
    D(HKEY_PERFORMANCE_NLSTEXT);
    D(HKEY_CURRENT_CONFIG);
    D(HKEY_DYN_DATA);
#undef D
    return mod;
}

void
Init_predefined_key(void)
{
}
