#ifndef PHP_VM_V2Z
#define PHP_VM_V2Z

#include <ruby.h>
#include <sapi/embed/php_embed.h>

extern void value_to_zval(VALUE v, zval **z);

#endif
