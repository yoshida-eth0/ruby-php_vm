#include "php_vm.h"
#include "php_vm_z2v.h"
#include "php_vm_v2z.h"
#include <string.h>


// global

VALUE rb_mPHPVM;
VALUE rb_mPHPGlobal;
VALUE rb_cPHPClass;
VALUE rb_cPHPObject;
VALUE rb_ePHPError;
VALUE rb_ePHPExceptionObject;
VALUE rb_ePHPSyntaxError;


// PHP

void php_eval_string(char *code, int code_len TSRMLS_DC)
{
	int syntax_error = 0;

	// eval
	zend_try {
		if (zend_eval_stringl(code, code_len, NULL, "php_vm" TSRMLS_CC)==FAILURE) {
			syntax_error = 1;
		}
	} zend_end_try();

	// syntax error
	if (syntax_error) {
		VALUE exception = rb_exc_new2(rb_ePHPSyntaxError, "Syntax error");
		rb_exc_raise(exception);
	}

	// exception
	if (EG(exception)) {
		VALUE exception = zval_to_value(EG(exception));
		EG(exception) = NULL;
		rb_exc_raise(exception);
	}

	// exit
	if (EG(exit_status)!=0) {
		int exit_status = EG(exit_status);
		EG(exit_status) = 0;

		char message[32];
		sprintf(message, "exit status error: %d", exit_status);

		VALUE exception = rb_exc_new2(rb_ePHPError, message);
		rb_exc_raise(exception);
	}
}

void find_zend_class_entry(char *name, int name_len, zend_class_entry ***ce)
{
	// lowercase
	char *lcname = malloc(name_len+1);
	memcpy(lcname, name, name_len);
	zend_str_tolower(lcname, name_len);
	lcname[name_len] = '\0';

	// find zend class
	*ce = NULL;
	zend_hash_find(CG(class_table), lcname, name_len+1, (void **)ce);
	if (*ce) {
		// check string case
		if (strcmp(name, (**ce)->name)!=0) {
			*ce = NULL;
		}
	}

	free(lcname);
}

int is_exception_zend_class_entry(zend_class_entry *ce TSRMLS_DC)
{
	return instanceof_function(ce, zend_exception_get_default() TSRMLS_CC);
}

int is_exception_zval(zval *z TSRMLS_DC)
{
	return is_exception_zend_class_entry(Z_OBJCE_P(z) TSRMLS_CC);
}

void find_zend_function(zend_class_entry *ce, char *name, int name_len, zend_function **mptr)
{
	// function_table
	HashTable *function_table = NULL;
	if (ce && &ce->function_table) {
		function_table = &ce->function_table;
	} else {
		function_table = EG(function_table);
	}

	// lowercase
	char *lcname = malloc(name_len+1);
	memcpy(lcname, name, name_len);
	zend_str_tolower(lcname, name_len);
	lcname[name_len] = '\0';

	// find zend function
	*mptr = NULL;
	zend_hash_find(function_table, lcname, name_len+1, (void **)mptr);
	if (*mptr) {
		// check string case
		if (strcmp(name, (*mptr)->common.function_name)!=0) {
			*mptr = NULL;
		}
	}

	free(lcname);
}

int new_php_object(zend_class_entry *ce, VALUE v_args, zval *retval)
{
	int result = FAILURE;

	if (ce->constructor) {
		TSRMLS_FETCH();

		// defined constructor
/*
		if (!(ce->constructor->common.fn_flags & ZEND_ACC_PUBLIC)) {
			char *message = malloc(50+strlen(ce->name));
			sprintf(message, "Access to non-public constructor of class %s", ce->name);
			VALUE v_exception = rb_exc_new2(rb_ePHPError, message);
			free(message);
			rb_exc_raise(v_exception);
		}
*/

		// alloc
		object_init_ex(retval, ce);

		// call
		int result = call_php_method(ce, retval, ce->constructor, RARRAY_LEN(v_args), RARRAY_PTR(v_args), &retval TSRMLS_CC);

		// error
		if (result==FAILURE) {
			char *message = malloc(40+strlen(ce->name));
			sprintf(message, "Invocation of %s's constructor failed", ce->name);
			VALUE v_exception = rb_exc_new2(rb_ePHPError, message);
			free(message);
			rb_exc_raise(v_exception);
		}

	} else if (!RARRAY_LEN(v_args)) {
		// undefined constructor, hasnt args
		object_init_ex(retval, ce);
		result = SUCCESS;

	} else {
		// undefine constructor, has args
		char *message = malloc(90+strlen(ce->name));
		sprintf(message, "Class %s does not have a constructor, so you cannot pass any constructor arguments", ce->name);
		VALUE v_exception = rb_exc_new2(rb_ePHPError, message);
		free(message);
		rb_exc_raise(v_exception);
	}
	return result;
}

void define_php_properties(VALUE v_obj, zend_class_entry *ce, int is_static)
{
	HashPosition pos;
	zend_property_info *prop;

	zend_hash_internal_pointer_reset_ex(&ce->properties_info, &pos);

	while (zend_hash_get_current_data_ex(&ce->properties_info, (void **) &prop, &pos) == SUCCESS) {
		int flag = prop->flags;
		const char *getter_name = prop->name;
		char *setter_name = malloc(prop->name_length+ 1);
		sprintf(setter_name, "%s=", getter_name);

		if (is_static) {
			// static variable
			if (0<(flag & ZEND_ACC_STATIC)) {
				rb_define_singleton_method(v_obj, getter_name, rb_php_class_getter, 0);
				rb_define_singleton_method(v_obj, setter_name, rb_php_class_setter, 1);
			}
		} else {
			// instance variable
			if (0==(flag & ZEND_ACC_STATIC)) {
				rb_define_singleton_method(v_obj, getter_name, rb_php_object_getter, 0);
				rb_define_singleton_method(v_obj, setter_name, rb_php_object_setter, 1);
			}
		}

		free(setter_name);
		zend_hash_move_forward_ex(&ce->properties_info, &pos);
	}
}

void define_php_methods(VALUE v_obj, zend_class_entry *ce, int is_static)
{
	// TODO: access scope
	// TODO: __toString
	// TODO: __clone
	// TODO: __call
	// TODO: __callStatic
	// TODO: __get
	// TODO: __set
	// TODO: __isset

	HashPosition pos;
	zend_function *mptr;

	zend_hash_internal_pointer_reset_ex(&ce->function_table, &pos);

	while (zend_hash_get_current_data_ex(&ce->function_table, (void **)&mptr, &pos) == SUCCESS) {
		int flag = mptr->common.fn_flags;
		const char *fname = mptr->common.function_name;

		if (is_static) {
			// class method
			if (strcmp("new", fname)==0) {
				// new => no define

			} else if (0<(flag & ZEND_ACC_STATIC)) {
				// other method
				rb_define_singleton_method(v_obj, fname, rb_php_class_call, -1);
			}
		} else {
			// instance method
			if (strcmp("__construct", fname)==0 || strcmp(ce->name, fname)==0) {
				// __construct => no define

			} else if (0==(flag & ZEND_ACC_STATIC)) {
				// other method
				rb_define_singleton_method(v_obj, fname, rb_php_object_call, -1);
			}
		}

		zend_hash_move_forward_ex(&ce->function_table, &pos);
	}
}

int call_php_method(zend_class_entry *ce, zval *obj, zend_function *mptr, int argc, VALUE *v_argv, zval **retval_ptr TSRMLS_DC)
{
	int result = FAILURE;

	// call info
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;
	zval *return_value;
	ALLOC_INIT_ZVAL(return_value);

	fci.size = sizeof(fci);
	fci.function_table = NULL;
	fci.function_name = NULL;
	fci.symbol_table = NULL;
	fci.object_ptr = obj;
	fci.retval_ptr_ptr = retval_ptr;
	fci.param_count = 0;
	fci.params = NULL;
	fci.no_separation = 0;

	fcc.initialized = 1;
	fcc.function_handler = mptr;
	fcc.calling_scope = ce ? ce : EG(scope);
	fcc.called_scope = ce ? ce : NULL;
	fcc.object_ptr = obj;

	// zval args
	zval *z_args;
	MAKE_STD_ZVAL(z_args);
	array_init(z_args);

	long i;
	for (i=0; i<argc; i++) {
		zval *new_var;
		MAKE_STD_ZVAL(new_var);
		value_to_zval(v_argv[i], new_var);

		zend_hash_next_index_insert(Z_ARRVAL_P(z_args), &new_var, sizeof(zval *), NULL);
	}

	zend_fcall_info_args(&fci, z_args);

	// call
	zend_try {
		result = zend_call_function(&fci, &fcc TSRMLS_CC);
	} zend_catch {
		// EG(exception) is NULL... Why??
		//printf("call_php_method exception: %p\n", EG(exception));
	} zend_end_try();

	// reference variable
	HashPosition pos;
	zval **arg;
	zend_arg_info *arg_info = mptr->common.arg_info;
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(z_args), &pos);
	for (i=0; i<argc && i<mptr->common.num_args; i++) {
		if (arg_info->pass_by_reference) {
			zend_hash_get_current_data_ex(Z_ARRVAL_P(z_args), (void *)&arg, &pos);
			value_copy(v_argv[i], zval_to_value(*arg));
		}
		zend_hash_move_forward_ex(Z_ARRVAL_P(z_args), &pos);
		arg_info++;
	}

	// release
	for (i=0; i<fci.param_count; i++) {
		zval_ptr_dtor(fci.params[i]);
	}
	zend_fcall_info_args_clear(&fci, 1);
	return result;
}


// Ruby

VALUE get_callee_name()
{
	VALUE backtrace_arr = rb_funcall(rb_mKernel, rb_intern("caller"), 1, INT2NUM(0));
	if (backtrace_arr!=Qnil) {
		VALUE backtrace = rb_funcall(backtrace_arr, rb_intern("first"), 0);
		if (backtrace!=Qnil) {
			VALUE backtrace_re = rb_funcall(rb_cRegexp, rb_intern("new"), 1, rb_str_new2("^(.+?):(\\d+)(?::in `(.*)')?"));
			VALUE m = rb_funcall(backtrace, rb_intern("match"), 1, backtrace_re);
			if (m!=Qnil) {
				return rb_funcall(m, rb_intern("[]"), 1, INT2NUM(3));
			}
		}
	}
	return Qnil;
}

VALUE call_php_method_bridge(zend_class_entry *ce, zval *obj, VALUE callee, int argc, VALUE *argv)
{
	TSRMLS_FETCH();

	// callee
	if (callee==Qnil) {
		VALUE exception = rb_exc_new2(rb_ePHPError, "callee is nil");
		rb_exc_raise(exception);
	}

	// method
	zend_function *mptr;
	find_zend_function(ce, RSTRING_PTR(callee), RSTRING_LEN(callee), &mptr);

	// call
	zval *z_val;
	if (mptr) {
		// call method
		int result = call_php_method(ce, obj, mptr, argc, argv, &z_val TSRMLS_CC);

		// exception
		if (result==FAILURE) {
			char *message = malloc(32+RSTRING_LEN(callee));
			sprintf(message, "raise exception: %s", RSTRING_PTR(callee));
			VALUE exception = rb_exc_new2(rb_ePHPError, message);
			free(message);
			rb_exc_raise(exception);
		}
	} else {
		// accessor
		VALUE is_setter = rb_funcall(callee, rb_intern("end_with?"), 1, rb_str_new2("="));

		if (is_setter) {
			// setter
			rb_funcall(callee, rb_intern("gsub!"), 2, rb_str_new2("="), rb_str_new2(""));
			MAKE_STD_ZVAL(z_val);
			value_to_zval(argv[0], z_val);

			if (obj) {
				// instance
				add_property_zval(obj, RSTRING_PTR(callee), z_val);
			} else {
				// static
				zend_update_static_property(ce, RSTRING_PTR(callee), RSTRING_LEN(callee), z_val TSRMLS_CC);
			}

			// release
			zval_ptr_dtor(&z_val);

			return Qnil;
		} else {
			// getter
			if (obj) {
				// instance
				z_val = zend_read_property(ce, obj, RSTRING_PTR(callee), RSTRING_LEN(callee), 0 TSRMLS_CC);
			} else {
				// static
				z_val = zend_read_static_property(ce, RSTRING_PTR(callee), RSTRING_LEN(callee), 0 TSRMLS_CC);
			}
		}
	}

	return zval_to_value(z_val);
}

void value_copy(VALUE dst, VALUE src)
{
	switch (TYPE(dst)) {
		case T_ARRAY:{
			if (TYPE(src)==T_ARRAY) {
				rb_funcall(dst, rb_intern("replace"), 1, src);
			} else {
				rb_funcall(dst, rb_intern("clear"), 0);
				rb_funcall(dst, rb_intern("<<"), 1, src);
			}
			break;
		}
		case T_HASH:{
			rb_funcall(dst, rb_intern("clear"), 0);
			if (TYPE(src)==T_HASH) {
				rb_funcall(dst, rb_intern("update"), 1, src);
			} else {
				rb_funcall(dst, rb_intern("[]"), 2, rb_str_new2("reference"), src);
			}
			break;
		}
		case T_STRING:{
			rb_funcall(dst, rb_intern("clear"), 0);
			if (TYPE(src)!=T_STRING) {
				src = rb_funcall(src, rb_intern("to_s"), 0);
			}
			rb_funcall(dst, rb_intern("concat"), 1, src);
			break;
		}
	}
}


// PHP Native resource

void php_native_resource_delete(PHPNativeResource *p)
{
	if (p->zobj) {
		zval_ptr_dtor(&p->zobj);
		p->zobj = NULL;
	}
	free(p);
}

zend_class_entry* get_zend_class_entry(VALUE self)
{
	VALUE resource = rb_iv_get(self, "php_native_resource");
	if (resource==Qnil) {
		return NULL;
	}

	PHPNativeResource *p;
	Data_Get_Struct(resource, PHPNativeResource, p);
	if (p) {
		return p->ce;
	}
	return NULL;
}

zval* get_zval(VALUE self)
{
	VALUE resource = rb_iv_get(self, "php_native_resource");
	if (resource==Qnil) {
		return NULL;
	}

	PHPNativeResource *p;
	Data_Get_Struct(resource, PHPNativeResource, p);
	if (p) {
		return p->zobj;
	}
	return NULL;
}


// module PHPVM

VALUE rb_php_vm_require(VALUE cls, VALUE filepath)
{
	StringValue(filepath);
	filepath = rb_funcall(filepath, rb_intern("gsub"), 2, rb_str_new2("\\"), rb_str_new2("\\\\"));
	filepath = rb_funcall(filepath, rb_intern("gsub"), 2, rb_str_new2("\""), rb_str_new2("\\\""));

	VALUE code = rb_str_new2("require \"");
	rb_str_cat(code, RSTRING_PTR(filepath), RSTRING_LEN(filepath));
	rb_str_cat2(code, "\";");

	php_eval_string(RSTRING_PTR(code), RSTRING_LEN(code));

	return Qnil;
}

VALUE rb_php_vm_require_once(VALUE cls, VALUE filepath)
{
	StringValue(filepath);
	filepath = rb_funcall(filepath, rb_intern("gsub"), 2, rb_str_new2("\\"), rb_str_new2("\\\\"));
	filepath = rb_funcall(filepath, rb_intern("gsub"), 2, rb_str_new2("\""), rb_str_new2("\\\""));

	VALUE code = rb_str_new2("require_once \"");
	rb_str_cat(code, RSTRING_PTR(filepath), RSTRING_LEN(filepath));
	rb_str_cat2(code, "\";");

	php_eval_string(RSTRING_PTR(code), RSTRING_LEN(code));

	return Qnil;
}

VALUE rb_php_vm_exec(VALUE cls, VALUE code)
{
	TSRMLS_FETCH();
	php_eval_string(RSTRING_PTR(code), RSTRING_LEN(code) TSRMLS_CC);
	return Qnil;
}

VALUE rb_php_vm_get_class(VALUE cls, VALUE v_class_name)
{
	return rb_php_class_get(rb_cPHPClass, v_class_name);
}

VALUE define_global_constants()
{
	TSRMLS_FETCH();

	// method
	zend_function *mptr;
	find_zend_function(NULL, "get_defined_constants", strlen("get_defined_constants"), &mptr);
	if (!mptr) {
		return Qfalse;
	}

	// call method
	zval *z_val;
	int result = call_php_method(NULL, NULL, mptr, 0, NULL, &z_val TSRMLS_CC);
	if (result==FAILURE) {
		return Qfalse;
	}

	HashTable* ht = Z_ARRVAL_P(z_val);
	HashPosition pos;
	zval** data;

	zend_hash_internal_pointer_reset_ex(ht, &pos);
	while (SUCCESS == zend_hash_get_current_data_ex(ht, (void **)&data, &pos)) {
		char *string_key;
		ulong num_index;

		switch(zend_hash_get_current_key_ex(ht, &string_key, NULL, &num_index, 0, &pos)) {
			case HASH_KEY_IS_STRING:{
				if (0x61<=string_key[0] && string_key[0]<=0x7a) {
					// lower case
					char *string_key2 = malloc(strlen(string_key));
					string_key2[0] = string_key2[0]-32;
					if (!rb_const_defined(rb_mPHPGlobal, rb_intern(string_key2))) {
						rb_define_const(rb_mPHPGlobal, string_key2, zval_to_value(*data));
					}
					free(string_key2);
				} else {
					// upper case
					if (!rb_const_defined(rb_mPHPGlobal, rb_intern(string_key))) {
						rb_define_const(rb_mPHPGlobal, string_key, zval_to_value(*data));
					}
				}
				break;
			}
		}

		zend_hash_move_forward_ex(ht, &pos);
	}
	return Qtrue;
}

VALUE define_global_functions()
{
	TSRMLS_FETCH();

	// method
	zend_function *mptr;
	find_zend_function(NULL, "get_defined_functions", strlen("get_defined_functions"), &mptr);
	if (!mptr) {
		return Qfalse;
	}

	// call method
	zval *z_val;
	int result = call_php_method(NULL, NULL, mptr, 0, NULL, &z_val TSRMLS_CC);
	if (result==FAILURE) {
		return Qfalse;
	}

	HashTable* ht = Z_ARRVAL_P(z_val);
	HashPosition pos;
	zval** data;

	zend_hash_internal_pointer_reset_ex(ht, &pos);
	while (SUCCESS == zend_hash_get_current_data_ex(ht, (void **)&data, &pos)) {
		HashTable* ht2 = Z_ARRVAL_P(*data);
		HashPosition pos2;
		zval** data2;

		zend_hash_internal_pointer_reset_ex(ht2, &pos2);
		while (SUCCESS == zend_hash_get_current_data_ex(ht2, (void **)&data2, &pos2)) {
			rb_define_module_function(rb_mPHPGlobal, Z_STRVAL_P(*data2), rb_php_global_function_call, -1);
			zend_hash_move_forward_ex(ht2, &pos2);
		}

		zend_hash_move_forward_ex(ht, &pos);
	}
	return Qtrue;
}

VALUE define_global_classes()
{
	TSRMLS_FETCH();

	// method
	zend_function *mptr;
	find_zend_function(NULL, "get_declared_classes", strlen("get_declared_classes"), &mptr);
	if (!mptr) {
		return Qfalse;
	}

	// call method
	zval *z_val;
	int result = call_php_method(NULL, NULL, mptr, 0, NULL, &z_val TSRMLS_CC);
	if (result==FAILURE) {
		return Qfalse;
	}

	HashTable* ht = Z_ARRVAL_P(z_val);
	HashPosition pos;
	zval** data;

	zend_hash_internal_pointer_reset_ex(ht, &pos);
	while (SUCCESS == zend_hash_get_current_data_ex(ht, (void **)&data, &pos)) {
		rb_define_module_function(rb_mPHPGlobal, Z_STRVAL_P(*data), rb_php_global_class_call, 0);
		zend_hash_move_forward_ex(ht, &pos);
	}
	return Qtrue;
}

VALUE rb_php_vm_define_global(VALUE cls)
{
	VALUE res1 = define_global_constants();
	VALUE res2 = define_global_functions();
	VALUE res3 = define_global_classes();
	return res1==Qtrue && res2==Qtrue && res3==Qtrue;
}


// module PHPVM::PHPGlobal

VALUE rb_php_global_function_call(int argc, VALUE *argv, VALUE self)
{
	VALUE callee = get_callee_name();
	return call_php_method_bridge(NULL, NULL, callee, argc, argv);
}

VALUE rb_php_global_class_call(VALUE self)
{
	VALUE callee = get_callee_name();
	return rb_php_class_get(rb_cPHPClass, callee);
}

VALUE rb_php_global_echo(int argc, VALUE *argv, VALUE self)
{
	int i;

	if (argc==0) {
		VALUE exception = rb_exc_new2(rb_eArgError, "Too few arguments");
		rb_exc_raise(exception);
	}

	// format
	char *format = malloc(argc*2+1);
	for (i=0; i<argc; i++) {
		format[i*2] = '%';
		format[i*2+1] = 's';
	}
	format[i*2] = '\0';

	// argv
	VALUE *argv2 = malloc(sizeof(VALUE)*(argc+1));
	argv2[0] = rb_str_new2(format);
	for (i=0; i<argc; i++) {
		argv2[i+1] = argv[i];
	}
	call_php_method_bridge(NULL, NULL, rb_str_new2("printf"), argc+1, argv2);

	// release
	free(format);
	free(argv2);
	return Qnil;
}

VALUE rb_php_global_array(int argc, VALUE *argv, VALUE self)
{
	VALUE result;
	if (argc==1 && TYPE(argv[0])==T_HASH) {
		// hash
		result = argv[0];
	} else {
		// argv
		rb_scan_args(argc, argv, "*", &result);
	}
	return result;
}


// class PHPVM::PHPClass

VALUE rb_php_class_get(VALUE cls, VALUE v_name)
{
	// find
	VALUE name_sym = rb_str_intern(v_name);
	VALUE classes = rb_cv_get(cls, "@@classes");
	VALUE class = rb_hash_lookup(classes, name_sym);

	if (class==Qnil) {
		// create
		class = rb_obj_alloc(rb_cPHPClass);
		rb_php_class_initialize(class, v_name);
		rb_hash_aset(classes, name_sym, class);
	}

	return class;
}

VALUE rb_php_class_initialize(VALUE self, VALUE v_name)
{
	rb_iv_set(self, "name", v_name);

	// find zend class
	zend_class_entry **ce = NULL;
	find_zend_class_entry(RSTRING_PTR(v_name), RSTRING_LEN(v_name), &ce);

	// class not found
	if (!ce) {
		char *message = malloc(32+RSTRING_LEN(v_name));
		sprintf(message, "Class is not found: %s", RSTRING_PTR(v_name));
		VALUE exception = rb_exc_new2(rb_ePHPError, message);
		free(message);
		rb_exc_raise(exception);
	}

	// set resource
	PHPNativeResource *p = ALLOC(PHPNativeResource);
	p->ce = *ce;
	p->zobj = NULL;
	VALUE resource = Data_Wrap_Struct(CLASS_OF(self), 0, php_native_resource_delete, p);
	rb_iv_set(self, "php_native_resource", resource);

	// define php static properties and methods
	define_php_properties(self, *ce, 1);
	define_php_methods(self, *ce, 1);

	return self;
}

VALUE rb_php_class_name(VALUE self)
{
	return rb_iv_get(self, "name");
}

VALUE rb_php_class_new(int argc, VALUE *argv, VALUE self)
{
	VALUE args;
	rb_scan_args(argc, argv, "*", &args);

	// alloc
	VALUE obj = Qnil;
	zend_class_entry *ce = get_zend_class_entry(self);
	if (is_exception_zend_class_entry(ce)) {
		obj = rb_obj_alloc(rb_ePHPExceptionObject);
	} else {
		obj = rb_obj_alloc(rb_cPHPObject);
	}
	rb_php_object_initialize(obj, self, args);

	// define php instance properties and methods
	define_php_properties(obj, ce, 0);
	define_php_methods(obj, ce, 0);

	return obj;
}

VALUE rb_php_class_getter(VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	VALUE callee = get_callee_name();
	return call_php_method_bridge(ce, NULL, callee, 0, NULL);
}

VALUE rb_php_class_setter(VALUE self, VALUE value)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	VALUE callee = get_callee_name();
	return call_php_method_bridge(ce, NULL, callee, 1, &value);
}

VALUE rb_php_class_call(int argc, VALUE *argv, VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	VALUE callee = get_callee_name();
	return call_php_method_bridge(ce, NULL, callee, argc, argv);
}


// class PHPVM::PHPObject

VALUE rb_php_object_initialize(VALUE self, VALUE class, VALUE args)
{
	// set class
	rb_iv_set(self, "php_class", class);

	// create php object
	zend_class_entry *ce = get_zend_class_entry(class);
	zval *z_obj;
	ALLOC_INIT_ZVAL(z_obj);
	new_php_object(ce, args, z_obj);
	
	// set resource
	PHPNativeResource *p = ALLOC(PHPNativeResource);
	p->ce = ce;
	p->zobj = z_obj;
	VALUE resource = Data_Wrap_Struct(CLASS_OF(self), 0, php_native_resource_delete, p);
	rb_iv_set(self, "php_native_resource", resource);

	return self;
}

VALUE rb_php_object_php_class(VALUE self)
{
	return rb_iv_get(self, "php_class");
}

VALUE rb_php_object_getter(VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	zval *zobj = get_zval(self);
	VALUE callee = get_callee_name();
	return call_php_method_bridge(ce, zobj, callee, 0, NULL);
}

VALUE rb_php_object_setter(VALUE self, VALUE value)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	zval *zobj = get_zval(self);
	VALUE callee = get_callee_name();
	return call_php_method_bridge(ce, zobj, callee, 1, &value);
}

VALUE rb_php_object_call(int argc, VALUE *argv, VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	zval *zobj = get_zval(self);
	VALUE callee = get_callee_name();
	return call_php_method_bridge(ce, zobj, callee, argc, argv);
}


// class PHPVM::PHPExceptionObject

VALUE rb_php_exception_object_initialize(int argc, VALUE *argv, VALUE self)
{
	rb_call_super(argc, argv);
	return self;
}


// module

void php_vm_module_init()
{
	TSRMLS_FETCH();
	int argc = 1;
	char *argv[2] = {"php_vm", NULL};
	php_embed_init(argc, argv PTSRMLS_CC);
	EG(bailout) = NULL;
}

void php_vm_module_exit()
{
	TSRMLS_FETCH();
	php_embed_shutdown(TSRMLS_C);
}

void Init_php_vm()
{
	// initialize php_vm
	php_vm_module_init();
	atexit(php_vm_module_exit);

	// module PHPVM
	rb_mPHPVM = rb_define_module("PHPVM");

	rb_define_singleton_method(rb_mPHPVM, "require", rb_php_vm_require, 1);
	rb_define_singleton_method(rb_mPHPVM, "require_once", rb_php_vm_require_once, 1);
	rb_define_singleton_method(rb_mPHPVM, "exec", rb_php_vm_exec, 1);
	rb_define_singleton_method(rb_mPHPVM, "get_class", rb_php_vm_get_class, 1);
	rb_define_singleton_method(rb_mPHPVM, "define_global", rb_php_vm_define_global, 0);

	rb_define_const(rb_mPHPVM, "VERSION", rb_str_new2("1.2.3"));

	// module PHPVM::PHPGlobal
	rb_mPHPGlobal = rb_define_module_under(rb_mPHPVM, "PHPGlobal");
	rb_define_module_function(rb_mPHPGlobal, "echo", rb_php_global_echo, -1);
	rb_define_module_function(rb_mPHPGlobal, "array", rb_php_global_array, -1);
	rb_php_vm_define_global(rb_mPHPVM);

	// class PHPVM::PHPClass
	rb_cPHPClass = rb_define_class_under(rb_mPHPVM, "PHPClass", rb_cObject);
	rb_define_class_variable(rb_cPHPClass, "@@classes", rb_obj_alloc(rb_cHash));

	rb_define_private_method(rb_cPHPClass, "initialize", rb_php_class_initialize, 1);
	rb_define_method(rb_cPHPClass, "name", rb_php_class_name, 0);
	rb_define_method(rb_cPHPClass, "new", rb_php_class_new, -1);
	rb_define_singleton_method(rb_cPHPClass, "get", rb_php_class_get, 1);

	// class PHPVM::PHPObject
	rb_cPHPObject = rb_define_class_under(rb_mPHPVM, "PHPObject", rb_cObject);

	rb_define_private_method(rb_cPHPObject, "initialize", rb_php_object_initialize, 1);
	rb_define_method(rb_cPHPObject, "php_class", rb_php_object_php_class, 0);

	// class PHPVM::PHPError < StandardError
	rb_ePHPError = rb_define_class_under(rb_mPHPVM, "PHPError", rb_eStandardError);

	// class PHPVM::PHPExceptionObject < PHPVM::PHPError
	rb_ePHPExceptionObject = rb_define_class_under(rb_mPHPVM, "PHPExceptionObject", rb_ePHPError);

	rb_define_method(rb_ePHPExceptionObject, "initialize", rb_php_exception_object_initialize, -1);
	rb_define_method(rb_ePHPExceptionObject, "php_class", rb_php_object_php_class, 0);

	// class PHPVM::PHPSyntaxError < PHPVM::PHPError
	rb_ePHPSyntaxError = rb_define_class_under(rb_mPHPVM, "PHPSyntaxError", rb_ePHPError);
}
