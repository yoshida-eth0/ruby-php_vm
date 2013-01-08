#ifndef PHP_VM_H
#define PHP_VM_H

#include <ruby.h>
#include <sapi/embed/php_embed.h>
#include <Zend/zend_execute.h>
#include <Zend/zend_exceptions.h>


extern VALUE rb_mPHPVM;
extern VALUE rb_cPHPClass;
extern VALUE rb_cPHPObject;
extern VALUE rb_ePHPExceptionObject;
extern VALUE rb_ePHPError;
extern VALUE rb_ePHPSyntaxError;

typedef struct {
	zend_class_entry *ce;
	zval *zobj;
} PHPNativeResource;

typedef struct {
	VALUE proc;
	VALUE args;
} HandlerArgs;

typedef struct {
	char *token;
	VALUE filepath;
} RequireArgs;


// PHP
extern VALUE php_eval_string(char *code, int code_len, int use_retval TSRMLS_DC);
extern void find_zend_class_entry(char *name, int name_len, zend_class_entry ***ce TSRMLS_DC);
extern int is_exception_zend_class_entry(zend_class_entry *ce TSRMLS_DC);
extern int is_exception_zval(zval *z TSRMLS_DC);
extern void find_zend_function(zend_class_entry *ce, char *name, int name_len, zend_function **mptr TSRMLS_DC);
extern int new_php_object(zend_class_entry *ce, VALUE v_args, zval *retval TSRMLS_DC);
extern void define_php_properties(VALUE v_obj, zend_class_entry *ce, int is_static);
extern void define_php_methods(VALUE v_obj, zend_class_entry *ce, int is_static);
extern void define_php_magic_method(VALUE v_obj, zend_class_entry *ce, zval *zobj);
extern int call_php_method(zend_class_entry *ce, zval *obj, zend_function *mptr, int argc, VALUE *v_argv, zval **retval_ptr TSRMLS_DC);

// Ruby
extern VALUE get_callee_name();
extern VALUE call_php_method_bridge(zend_class_entry *ce, zval *obj, zend_function *mptr, int argc, VALUE *argv);
extern VALUE call_php_method_name_bridge(zend_class_entry *ce, zval *obj, VALUE callee, int argc, VALUE *argv);
extern void value_copy(VALUE dst, VALUE src);

// PHP Native resource
extern void php_native_resource_delete(PHPNativeResource *p);
extern zend_class_entry* get_zend_class_entry(VALUE self);
extern zval* get_zval(VALUE self);

// module PHPVM
extern VALUE rb_php_vm_get_output_handler(VALUE cls);
extern VALUE rb_php_vm_set_output_handler(VALUE cls, VALUE proc);
extern VALUE rb_php_vm_get_error_handler(VALUE cls);
extern VALUE rb_php_vm_set_error_handler(VALUE cls, VALUE proc);
extern VALUE php_vm_require(char *token, VALUE filepath TSRMLS_DC);
extern VALUE rb_php_vm_require(VALUE cls, VALUE filepath);
extern VALUE rb_php_vm_require_once(VALUE cls, VALUE filepath);
extern VALUE rb_php_vm_include(VALUE cls, VALUE filepath);
extern VALUE rb_php_vm_include_once(VALUE cls, VALUE filepath);
extern VALUE rb_php_vm_exec(VALUE cls, VALUE code);
extern VALUE rb_php_vm_get_class(VALUE cls, VALUE v_class_name);
extern VALUE define_global_constants();
extern VALUE define_global_functions();
extern VALUE define_global_classes();
extern VALUE rb_php_vm_define_global(VALUE cls);

// module PHPVM::PHPGlobal
extern VALUE rb_php_global_function_call(int argc, VALUE *argv, VALUE self);
extern VALUE rb_php_global_class_call(VALUE self);
extern VALUE php_global_require(char *token, VALUE filepath);
extern VALUE rb_php_global_require(VALUE cls, VALUE filepath);
extern VALUE rb_php_global_require_once(VALUE cls, VALUE filepath);
extern VALUE rb_php_global_echo(int argc, VALUE *argv, VALUE cls);
extern VALUE rb_php_global_print(VALUE self, VALUE cls);
extern VALUE rb_php_global_array(int argc, VALUE *argv, VALUE cls);

// class PHPVM::PHPClass
extern VALUE rb_php_class_get(VALUE cls, VALUE v_name);
extern VALUE rb_php_class_initialize(VALUE self, VALUE v_name);
extern VALUE rb_php_class_name(VALUE self);
extern VALUE rb_php_class_new(int argc, VALUE *argv, VALUE self);
extern VALUE rb_php_class_getter(VALUE self);
extern VALUE rb_php_class_setter(VALUE self, VALUE value);
extern VALUE rb_php_class_call(int argc, VALUE *argv, VALUE self);
extern VALUE rb_php_class_call_magic___callstatic(VALUE self, VALUE name, VALUE args);
extern VALUE rb_php_class_call_method_missing(int argc, VALUE *argv, VALUE self);

// class PHPVM::PHPObject
extern VALUE rb_php_object_initialize(VALUE self, VALUE class, VALUE arg_arr);
extern VALUE rb_php_object_php_class(VALUE self);
extern VALUE rb_php_object_getter(VALUE self);
extern VALUE rb_php_object_setter(VALUE self, VALUE value);
extern VALUE rb_php_object_call(int argc, VALUE *argv, VALUE self);
extern VALUE rb_php_object_call_magic_clone(VALUE self);
extern VALUE rb_php_object_call_magic___get(VALUE self, VALUE name);
extern VALUE rb_php_object_call_magic___set(VALUE self, VALUE name, VALUE arg);
extern VALUE rb_php_object_call_magic___unset(VALUE self, VALUE name);
extern VALUE rb_php_object_call_magic___isset(VALUE self, VALUE name);
extern VALUE rb_php_object_call_magic___call(VALUE self, VALUE name, VALUE args);
extern VALUE rb_php_object_call_magic___tostring(VALUE self);
extern VALUE rb_php_object_call_method_missing(int argc, VALUE *argv, VALUE self);

// class PHPVM::PHPExceptionObject
extern VALUE rb_php_exception_object_initialize(int argc, VALUE *argv, VALUE self);

// class PHPVM::PHPErrorReporting
extern VALUE rb_php_error_reporting_initialize(int argc, VALUE *argv, VALUE self);
extern VALUE rb_php_error_reporting_log_message(VALUE self);
extern VALUE rb_php_error_reporting_error_level(VALUE self);
extern VALUE rb_php_error_reporting_file(VALUE self);
extern VALUE rb_php_error_reporting_line(VALUE self);

// module
extern void php_vm_module_exit();
extern void Init_php_vm();

#endif
