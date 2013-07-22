#include <ruby.h>

#ifndef HKEY_PERFORMANCE_TEXT
#define HKEY_PERFORMANCE_TEXT 0x80000050
#endif
#ifndef HKEY_PERFORMANCE_NLSTEXT
#define HKEY_PERFORMANCE_NLSTEXT 0x80000060
#endif

#ifndef REG_QWORD_LITTLE_ENDIAN
#define REG_QWORD_LITTLE_ENDIAN 11
#endif
#ifndef REG_QWORD
#define REG_QWORD REG_QWORD_LITTLE_ENDIAN
#endif

#ifndef REG_FORCE_RESTORE
#define REG_FORCE_RESTORE 0x0008
#endif

VALUE InitVM_registry_constants(VALUE mod);
void Init_registry_constants(void);

typedef VALUE predefine_key_func(VALUE klass, HANDLE hkey, ID name);
VALUE InitVM_predefined_key(VALUE mod, predefine_key_func *predefine_key);
void Init_predefined_key(void);

VALUE InitVM_registry_error(VALUE mod);
void Init_registry_error(void);
void reg_error_check(DWORD code, VALUE name);


