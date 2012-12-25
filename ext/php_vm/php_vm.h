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


// PHP
extern void php_eval_string(char *code, int code_len TSRMLS_DC);
extern void find_zend_class_entry(char *name, int name_len, zend_class_entry ***ce);
extern void find_zend_class_entry2(char *name, zend_class_entry ***ce);
extern int is_exception_zend_class_entry(zend_class_entry *ce TSRMLS_DC);
extern int is_exception_zval(zval *z TSRMLS_DC);
extern int new_php_object(zend_class_entry *ce, VALUE v_args, zval *retval);
extern void define_php_properties(VALUE v_obj, zend_class_entry *ce, int is_static);
extern void define_php_methods(VALUE v_obj, zend_class_entry *ce, int is_static);
extern int call_php_method(zend_class_entry *ce, zval *obj, zend_function *mptr, int argc, VALUE *v_argv, zval **retval_ptr TSRMLS_DC);

// Ruby
extern VALUE get_callee_name();
extern VALUE call_php_method_bridge(zend_class_entry *ce, zval *obj, VALUE callee, int argc, VALUE *argv);

// PHP Native resource
extern void php_native_resource_delete(PHPNativeResource *p);
extern zend_class_entry* get_zend_class_entry(VALUE self);
extern zval* get_zval(VALUE self);

// module PHPVM
extern VALUE rb_php_vm_require(VALUE cls, VALUE rbv_filepath);
extern VALUE rb_php_vm_exec(VALUE cls, VALUE rbv_code);
extern VALUE rb_php_vm_get_class(VALUE cls, VALUE rbv_class_name);
extern VALUE rb_php_vm_define_global_constants(VALUE cls);
extern VALUE rb_php_vm_define_global_functions(VALUE cls);
extern VALUE rb_php_vm_define_global_classes(VALUE cls);
extern VALUE rb_php_vm_define_global(VALUE cls);

// module PHPVM::PHPGlobal
extern VALUE rb_php_global_function_call(int argc, VALUE *argv, VALUE self);
extern VALUE rb_php_global_class_call(VALUE self);

// class PHPVM::PHPClass
extern VALUE rb_php_class_get(VALUE cls, VALUE rbv_name);
extern VALUE rb_php_class_initialize(VALUE self, VALUE rbv_name);
extern VALUE rb_php_class_name(VALUE self);
extern VALUE rb_php_class_new(int argc, VALUE *argv, VALUE self);
extern VALUE rb_php_class_getter(VALUE self);
extern VALUE rb_php_class_setter(VALUE self, VALUE value);
extern VALUE rb_php_class_call(int argc, VALUE *argv, VALUE self);

// class PHPVM::PHPObject
extern VALUE rb_php_object_initialize(VALUE self, VALUE class, VALUE arg_arr);
extern VALUE rb_php_object_php_class(VALUE self);
extern VALUE rb_php_object_getter(VALUE self);
extern VALUE rb_php_object_setter(VALUE self, VALUE value);
extern VALUE rb_php_object_call(int argc, VALUE *argv, VALUE self);

// module
extern void php_vm_module_init();
extern void php_vm_module_exit();
extern void Init_php_vm();

#endif
