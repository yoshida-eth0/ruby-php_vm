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
VALUE rb_ePHPErrorReporting;


// PHP Embed

static VALUE php_vm_handler_b_proc(HandlerArgs *args)
{
	rb_proc_call(args->proc, args->args);
	return Qnil;
}

static VALUE php_vm_handler_r_proc(HandlerArgs *args, VALUE e)
{
	fprintf(stderr, "Exception has occurred in php_vm handler. php_vm handler must not occur exception.\n");
	return Qnil;
}

static int php_embed_output_handler(const char *str, unsigned int str_length TSRMLS_DC)
{
	VALUE proc = rb_cv_get(rb_mPHPVM, "@@output_handler");
	if (rb_obj_is_kind_of(proc, rb_cProc)) {
		VALUE args = rb_ary_new();
		rb_ary_push(args, rb_str_new(str, str_length));

		HandlerArgs handargs;
		handargs.proc = proc;
		handargs.args = args;
		rb_rescue(php_vm_handler_b_proc, (VALUE)&handargs, php_vm_handler_r_proc, (VALUE)&handargs);
	} else {
		printf("%s", str);
	}
	return str_length;
}

static void php_embed_error_handler(char *message TSRMLS_DC)
{
	VALUE report = rb_exc_new2(rb_ePHPErrorReporting, message);
	VALUE proc = rb_cv_get(rb_mPHPVM, "@@error_handler");
	if (rb_obj_is_kind_of(proc, rb_cProc)) {
		VALUE args = rb_ary_new();
		rb_ary_push(args, report);

		HandlerArgs handargs;
		handargs.proc = proc;
		handargs.args = args;
		rb_rescue(php_vm_handler_b_proc, (VALUE)&handargs, php_vm_handler_r_proc, (VALUE)&handargs);
	}
	if (rb_funcall(report, rb_intern("error_level"), 0)==ID2SYM(rb_intern("Fatal"))) {
		rb_cv_set(rb_mPHPVM, "@@last_error_reporting", report);
	}
}

static zval *g_exception = NULL;
static void php_vm_exception_hook(zval *ex TSRMLS_DC)
{
	// TODO: no use global variable
	g_exception = ex;
	EG(exception) = NULL;
}

static void php_vm_reset_status(TSRMLS_D)
{
	EG(exit_status) = 0;
	EG(exception) = NULL;
	rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
}


// PHP

VALUE php_eval_string(char *code, int code_len, int use_retval TSRMLS_DC)
{
	int syntax_error = 0;
	zval retval;

	// reset
	php_vm_reset_status(TSRMLS_C);

	// eval
	zend_try {
		if (zend_eval_stringl(code, code_len, (use_retval ? &retval : NULL), "php_vm" TSRMLS_CC)==FAILURE) {
			syntax_error = 1;
		}
	} zend_end_try();
	EG(active_op_array) = NULL;

	// syntax error
	if (syntax_error) {
		rb_raise(rb_ePHPSyntaxError, "Syntax error");
	}

	// exception
	if (g_exception) {
		VALUE exception = zval_to_value(g_exception);
		zval_ptr_dtor(&g_exception);
		g_exception = NULL;
		rb_exc_raise(exception);
	}

	// exit
	if (EG(exit_status)!=0) {
		int exit_status = EG(exit_status);
		EG(exit_status) = 0;

		VALUE report = rb_cv_get(rb_mPHPVM, "@@last_error_reporting");
		rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
		if (report!=Qnil) {
			rb_exc_raise(report);
		} else {
			rb_raise(rb_ePHPError, "exit status error: %d", exit_status);
		}
	}

	// return
	if (use_retval) {
		return zval_to_value(&retval);
	}
	return Qnil;
}

void find_zend_class_entry(char *name, int name_len, zend_class_entry ***ce TSRMLS_DC)
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
	return instanceof_function(ce, zend_exception_get_default(TSRMLS_C) TSRMLS_CC);
}

int is_exception_zval(zval *z TSRMLS_DC)
{
	return is_exception_zend_class_entry(Z_OBJCE_P(z) TSRMLS_CC);
}

void find_zend_function(zend_class_entry *ce, char *name, int name_len, zend_function **mptr TSRMLS_DC)
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

int new_php_object(zend_class_entry *ce, VALUE v_args, zval *retval TSRMLS_DC)
{
	int result = FAILURE;

	if (ce->constructor) {
		// defined constructor
/*
		if (!(ce->constructor->common.fn_flags & ZEND_ACC_PUBLIC)) {
			rb_raise(rb_ePHPError, "Access to non-public constructor of class %s", ce->name);
		}
*/

		// alloc
		object_init_ex(retval, ce);

		// call constructor
		result = call_php_method(ce, retval, ce->constructor, RARRAY_LEN(v_args), RARRAY_PTR(v_args), &retval TSRMLS_CC);

		// exception
		if (result==FAILURE) {
			VALUE report = rb_cv_get(rb_mPHPVM, "@@last_error_reporting");
			rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
			if (g_exception) {
				VALUE exception = zval_to_value(g_exception);
				zval_ptr_dtor(&g_exception);
				g_exception = NULL;
				rb_exc_raise(exception);
			} else if (report!=Qnil) {
				rb_exc_raise(report);
			} else {
				rb_raise(rb_ePHPError, "Invocation of %s's constructor failed", ce->name);
			}
		}

		zval_ptr_dtor(&retval);

	} else if (!RARRAY_LEN(v_args)) {
		// undefined constructor, hasnt args
		object_init_ex(retval, ce);
		result = SUCCESS;

	} else {
		// undefine constructor, has args
		rb_raise(rb_ePHPError, "Class %s does not have a constructor, so you cannot pass any constructor arguments", ce->name);
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
		char *setter_name = malloc(prop->name_length + 2);
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

	HashPosition pos;
	zend_function *mptr;

	zend_hash_internal_pointer_reset_ex(&ce->function_table, &pos);

	while (zend_hash_get_current_data_ex(&ce->function_table, (void **)&mptr, &pos) == SUCCESS) {
		int flag = mptr->common.fn_flags;
		const char *fname = mptr->common.function_name;
		int is_pass_define = 0;

		if (is_static) {
			// class method
			if (0<(flag & ZEND_ACC_STATIC)) {
				if (strcmp("new", fname)==0) {
					// new => no define
					is_pass_define = 1;

				} else if (strcmp("__callStatic", fname)==0) {
					// __callStatic => method_missing
					rb_define_singleton_method(v_obj, "method_missing", rb_php_class_call_method_missing, -1);
				}

				// define
				if (!is_pass_define) {
					rb_define_singleton_method(v_obj, fname, rb_php_class_call, -1);
				}
			}
		} else {
			// instance method
			if (0==(flag & ZEND_ACC_STATIC)) {
				if (strcmp("__construct", fname)==0 || strcmp(ce->name, fname)==0) {
					// __construct => no define
					is_pass_define = 1;
				}

				// other method
				if (!is_pass_define) {
					rb_define_singleton_method(v_obj, fname, rb_php_object_call, -1);
				}
			}
		}

		zend_hash_move_forward_ex(&ce->function_table, &pos);
	}
}

void define_php_magic_method(VALUE v_obj, zend_class_entry *ce, zval *zobj)
{
	if (!zobj) {
		// static method
		if (ce->__callstatic) {
			rb_define_singleton_method(v_obj, "__callStatic", rb_php_class_call_magic___callstatic, 2);
			rb_define_singleton_method(v_obj, "method_missing", rb_php_class_call_method_missing, -1);
		}
	} else {
		// instance method
		const zend_object_handlers *h = Z_OBJ_HT_P(zobj);
		int has_get = h->read_property!=std_object_handlers.read_property || (h->read_property && ce->__get);
		int has_set = h->write_property!=std_object_handlers.write_property || (h->write_property &&ce->__set);
		int has_unset = h->unset_property!=std_object_handlers.unset_property || (h->unset_property && ce->__unset);
		int has_isset = h->has_property!=std_object_handlers.has_property || (h->has_property && ce->__isset);

		if (ce->clone) {
			rb_define_singleton_method(v_obj, "__clone", rb_php_object_call_magic_clone, 0);
			rb_define_singleton_method(v_obj, "dup", rb_php_object_call_magic_clone, 0);
			rb_define_singleton_method(v_obj, "clone", rb_php_object_call_magic_clone, 0);
		}
		if (has_get) {
			rb_define_singleton_method(v_obj, "__get", rb_php_object_call_magic___get, 1);
		}
		if (has_set) {
			rb_define_singleton_method(v_obj, "__set", rb_php_object_call_magic___set, 2);
		}
		if (has_unset) {
			rb_define_singleton_method(v_obj, "__unset", rb_php_object_call_magic___unset, 1);
		}
		if (has_isset) {
			rb_define_singleton_method(v_obj, "__isset", rb_php_object_call_magic___isset, 1);
		}
		if (ce->__call) {
			rb_define_singleton_method(v_obj, "__call", rb_php_object_call_magic___call, 2);
		}
		if (ce->__tostring) {
			rb_define_singleton_method(v_obj, "__toString", rb_php_object_call_magic___tostring, 0);
			rb_define_singleton_method(v_obj, "to_s", rb_php_object_call_magic___tostring, 0);
		}

		if (has_get || has_set || ce->__call) {
			rb_define_singleton_method(v_obj, "method_missing", rb_php_object_call_method_missing, -1);
		}
	}
}

int call_php_method(zend_class_entry *ce, zval *obj, zend_function *mptr, int argc, VALUE *v_argv, zval **retval_ptr TSRMLS_DC)
{
	int result = FAILURE;

	// reset
	php_vm_reset_status(TSRMLS_C);

	// call info
	zend_fcall_info fci;
	zend_fcall_info_cache fcc;

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
		value_to_zval(v_argv[i], &new_var);

		zend_hash_next_index_insert(Z_ARRVAL_P(z_args), &new_var, sizeof(zval *), NULL);
	}

	zend_fcall_info_args(&fci, z_args TSRMLS_CC);

	zend_try {
		// call method
		result = zend_call_function(&fci, &fcc TSRMLS_CC);

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
	} zend_catch {
	} zend_end_try();

	// release
	zend_fcall_info_args_clear(&fci, 1);
	zval_ptr_dtor(&z_args);
	EG(active_op_array) = NULL;

	// result
	if (g_exception || rb_cv_get(rb_mPHPVM, "@@last_error_reporting")!=Qnil) {
		result = FAILURE;
	}
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

VALUE call_php_method_bridge(zend_class_entry *ce, zval *obj, zend_function *mptr, int argc, VALUE *argv)
{
	TSRMLS_FETCH();

	// call
	zval *z_val;
	int result = call_php_method(ce, obj, mptr, argc, argv, &z_val TSRMLS_CC);

	// exception
	if (result==FAILURE) {
		VALUE report = rb_cv_get(rb_mPHPVM, "@@last_error_reporting");
		rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
		if (g_exception) {
			VALUE exception = zval_to_value(g_exception);
			zval_ptr_dtor(&g_exception);
			g_exception = NULL;
			rb_exc_raise(exception);
		} else if (report!=Qnil) {
			rb_exc_raise(report);
		} else {
			rb_raise(rb_ePHPError, "raise exception: %s", mptr->common.function_name);
		}
	}

	VALUE v_retval = zval_to_value(z_val);
	zval_ptr_dtor(&z_val);
	return v_retval;
}

VALUE call_php_method_name_bridge(zend_class_entry *ce, zval *obj, VALUE callee, int argc, VALUE *argv)
{
	TSRMLS_FETCH();

	// callee
	if (callee==Qnil) {
		rb_raise(rb_ePHPError, "callee is nil");
	}

	// method
	zend_function *mptr;
	find_zend_function(ce, RSTRING_PTR(callee), RSTRING_LEN(callee), &mptr TSRMLS_CC);

	// call
	if (mptr) {
		// call method
		return call_php_method_bridge(ce, obj, mptr, argc, argv);
	} else {
		// accessor
		VALUE is_setter = rb_funcall(callee, rb_intern("end_with?"), 1, rb_str_new2("="));
		zval *z_val;

		if (is_setter) {
			// setter
			rb_funcall(callee, rb_intern("gsub!"), 2, rb_str_new2("="), rb_str_new2(""));
			value_to_zval(argv[0], &z_val);

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
			return zval_to_value(z_val);
		}
	}

	return Qnil;
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

VALUE rb_php_vm_get_output_handler(VALUE cls)
{
	return rb_cv_get(rb_mPHPVM, "@@output_handler");
}

VALUE rb_php_vm_set_output_handler(VALUE cls, VALUE proc)
{
	if (proc!=Qnil && !rb_obj_is_kind_of(proc, rb_cProc)) {
		rb_raise(rb_eArgError, "proc is not proc object");
	}
	rb_cv_set(rb_mPHPVM, "@@output_handler", proc);
	return Qnil;
}

VALUE rb_php_vm_get_error_handler(VALUE cls)
{
	return rb_cv_get(rb_mPHPVM, "@@error_handler");
}

VALUE rb_php_vm_set_error_handler(VALUE cls, VALUE proc)
{
	if (proc!=Qnil && !rb_obj_is_kind_of(proc, rb_cProc)) {
		rb_raise(rb_eArgError, "proc is not proc object");
	}
	rb_cv_set(rb_mPHPVM, "@@error_handler", proc);
	return Qnil;
}

VALUE php_vm_require(char *token, VALUE filepath TSRMLS_DC)
{
	StringValue(filepath);
	filepath = rb_funcall(filepath, rb_intern("gsub"), 2, rb_str_new2("\\"), rb_str_new2("\\\\"));
	filepath = rb_funcall(filepath, rb_intern("gsub"), 2, rb_str_new2("\""), rb_str_new2("\\\""));

	VALUE code = rb_str_new2(token);
	rb_str_cat2(code, " \"");
	rb_str_cat(code, RSTRING_PTR(filepath), RSTRING_LEN(filepath));
	rb_str_cat2(code, "\";");

	VALUE retval = php_eval_string(RSTRING_PTR(code), RSTRING_LEN(code), 1 TSRMLS_CC);
	switch (TYPE(retval)) {
		case T_TRUE:
		case T_FALSE:
		case T_NIL:
			break;
		default:{
			retval = rb_funcall(retval, rb_intern("to_i"), 0);
			retval = rb_funcall(retval, rb_intern(">"), 1, INT2NUM(0));
			break;
		}
	}
	return retval==Qtrue ? Qtrue : Qfalse;
}

VALUE rb_php_vm_require(VALUE cls, VALUE filepath)
{
	TSRMLS_FETCH();
	return php_vm_require("require", filepath TSRMLS_CC);
}

VALUE rb_php_vm_require_once(VALUE cls, VALUE filepath)
{
	TSRMLS_FETCH();
	return php_vm_require("require_once", filepath TSRMLS_CC);
}

VALUE rb_php_vm_include(VALUE cls, VALUE filepath)
{
	TSRMLS_FETCH();
	return php_vm_require("include", filepath TSRMLS_CC);
}

VALUE rb_php_vm_include_once(VALUE cls, VALUE filepath)
{
	TSRMLS_FETCH();
	return php_vm_require("include_once", filepath TSRMLS_CC);
}

VALUE rb_php_vm_exec(VALUE cls, VALUE code)
{
	TSRMLS_FETCH();
	php_eval_string(RSTRING_PTR(code), RSTRING_LEN(code), 0 TSRMLS_CC);
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
	find_zend_function(NULL, "get_defined_constants", strlen("get_defined_constants"), &mptr TSRMLS_CC);
	if (!mptr) {
		return Qfalse;
	}

	// call method
	zval *z_val;
	int result = call_php_method(NULL, NULL, mptr, 0, NULL, &z_val TSRMLS_CC);
	if (result==FAILURE) {
		if (g_exception) {
			zval_ptr_dtor(&g_exception);
			g_exception = NULL;
		}
		rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
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
					char *string_key2 = malloc(strlen(string_key)+1);
					memcpy(string_key2, string_key, strlen(string_key));
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
	zval_ptr_dtor(&z_val);
	return Qtrue;
}

VALUE define_global_functions()
{
	TSRMLS_FETCH();

	// method
	zend_function *mptr;
	find_zend_function(NULL, "get_defined_functions", strlen("get_defined_functions"), &mptr TSRMLS_CC);
	if (!mptr) {
		return Qfalse;
	}

	// call method
	zval *z_val;
	int result = call_php_method(NULL, NULL, mptr, 0, NULL, &z_val TSRMLS_CC);
	if (result==FAILURE) {
		if (g_exception) {
			zval_ptr_dtor(&g_exception);
			g_exception = NULL;
		}
		rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
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
	zval_ptr_dtor(&z_val);
	return Qtrue;
}

VALUE define_global_classes()
{
	TSRMLS_FETCH();

	// method
	zend_function *mptr;
	find_zend_function(NULL, "get_declared_classes", strlen("get_declared_classes"), &mptr TSRMLS_CC);
	if (!mptr) {
		return Qfalse;
	}

	// call method
	zval *z_val;
	int result = call_php_method(NULL, NULL, mptr, 0, NULL, &z_val TSRMLS_CC);
	if (result==FAILURE) {
		if (g_exception) {
			zval_ptr_dtor(&g_exception);
			g_exception = NULL;
		}
		rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
		return Qfalse;
	}

	HashTable* ht = Z_ARRVAL_P(z_val);
	HashPosition pos;
	zval** z_classname;

	zend_hash_internal_pointer_reset_ex(ht, &pos);
	while (SUCCESS == zend_hash_get_current_data_ex(ht, (void **)&z_classname, &pos)) {
		char *classname = Z_STRVAL_P(*z_classname);
		int classname_len = Z_STRLEN_P(*z_classname);

		if (0x41<=classname[0] && classname[0]<=0x5a) {
			// define class as const if class name is uppercase
			if (!rb_const_defined(rb_mPHPGlobal, rb_intern(classname))) {
				VALUE class = rb_php_class_get(rb_cPHPClass, rb_str_new(classname, classname_len));
				rb_define_const(rb_mPHPGlobal, classname, class);
			}
		} else {
			// define class as module function if class name is lowercase
			rb_define_module_function(rb_mPHPGlobal, classname, rb_php_global_class_call, 0);
		}

		zend_hash_move_forward_ex(ht, &pos);
	}
	zval_ptr_dtor(&z_val);
	return Qtrue;
}

VALUE rb_php_vm_define_global(VALUE cls)
{
	VALUE res1 = define_global_constants();
	VALUE res2 = define_global_functions();
	VALUE res3 = define_global_classes();
	return (res1==Qtrue && res2==Qtrue && res3==Qtrue) ? Qtrue : Qfalse;
}


// module PHPVM::PHPGlobal

VALUE rb_php_global_function_call(int argc, VALUE *argv, VALUE self)
{
	VALUE callee = get_callee_name();
	return call_php_method_name_bridge(NULL, NULL, callee, argc, argv);
}

VALUE rb_php_global_class_call(VALUE self)
{
	VALUE callee = get_callee_name();
	return rb_php_class_get(rb_cPHPClass, callee);
}

static VALUE php_global_require_b_proc(RequireArgs *args)
{
	TSRMLS_FETCH();
	return php_vm_require(args->token, args->filepath TSRMLS_CC);
}

static VALUE php_global_require_r_proc(RequireArgs *args, VALUE e)
{
	return rb_funcall(Qnil, rb_intern("require"), 1, args->filepath);
}

VALUE php_global_require(char *token, VALUE filepath)
{
	RequireArgs args;
	args.token = token;
	args.filepath = filepath;

	return rb_rescue(php_global_require_b_proc, (VALUE)&args, php_global_require_r_proc, (VALUE)&args);
}

VALUE rb_php_global_require(VALUE cls, VALUE filepath)
{
	return php_global_require("require", filepath);
}

VALUE rb_php_global_require_once(VALUE cls, VALUE filepath)
{
	return php_global_require("require_once", filepath);
}

VALUE rb_php_global_echo(int argc, VALUE *argv, VALUE cls)
{
	int i;

	if (argc==0) {
		rb_raise(rb_eArgError, "Too few arguments");
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
	call_php_method_name_bridge(NULL, NULL, rb_str_new2("printf"), argc+1, argv2);

	// release
	free(format);
	free(argv2);

	return Qnil;
}

VALUE rb_php_global_print(VALUE cls, VALUE arg)
{
	return rb_php_global_echo(1, &arg, cls);
}

VALUE rb_php_global_array(int argc, VALUE *argv, VALUE cls)
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
	TSRMLS_FETCH();

	rb_iv_set(self, "name", v_name);

	// find zend class
	zend_class_entry **ce = NULL;
	find_zend_class_entry(RSTRING_PTR(v_name), RSTRING_LEN(v_name), &ce TSRMLS_CC);

	// class not found
	if (!ce) {
		rb_raise(rb_ePHPError, "Class is not found: %s", RSTRING_PTR(v_name));
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
	define_php_magic_method(self, *ce, NULL);

	return self;
}

VALUE rb_php_class_name(VALUE self)
{
	return rb_iv_get(self, "name");
}

VALUE rb_php_class_new(int argc, VALUE *argv, VALUE self)
{
	TSRMLS_FETCH();

	VALUE args;
	rb_scan_args(argc, argv, "*", &args);

	// alloc
	VALUE obj = Qnil;
	zend_class_entry *ce = get_zend_class_entry(self);
	if (is_exception_zend_class_entry(ce TSRMLS_CC)) {
		obj = rb_obj_alloc(rb_ePHPExceptionObject);
	} else {
		obj = rb_obj_alloc(rb_cPHPObject);
	}
	rb_php_object_initialize(obj, self, args);

	return obj;
}

VALUE rb_php_class_getter(VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	VALUE callee = get_callee_name();
	return call_php_method_name_bridge(ce, NULL, callee, 0, NULL);
}

VALUE rb_php_class_setter(VALUE self, VALUE value)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	VALUE callee = get_callee_name();
	return call_php_method_name_bridge(ce, NULL, callee, 1, &value);
}

VALUE rb_php_class_call(int argc, VALUE *argv, VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	VALUE callee = get_callee_name();
	return call_php_method_name_bridge(ce, NULL, callee, argc, argv);
}

VALUE rb_php_class_call_magic___callstatic(VALUE self, VALUE name, VALUE args)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	zval *zobj = get_zval(self);
	VALUE argv[2] = {name, args};

	return call_php_method_bridge(ce, zobj, ce->__callstatic, 2, argv);
}

VALUE rb_php_class_call_method_missing(int argc, VALUE *argv, VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);

	VALUE name, args;
	rb_scan_args(argc, argv, "1*", &name, &args);
	VALUE argv2[2] = {name, args};

	return call_php_method_bridge(ce, NULL, ce->__callstatic, 2, argv2);
}


// class PHPVM::PHPObject

VALUE rb_php_object_initialize(VALUE self, VALUE class, VALUE args)
{
	TSRMLS_FETCH();

	// set class
	rb_iv_set(self, "php_class", class);

	// create php object
	zend_class_entry *ce = get_zend_class_entry(class);
	zval *z_obj;
	ALLOC_INIT_ZVAL(z_obj);
	new_php_object(ce, args, z_obj TSRMLS_CC);
	
	// set resource
	PHPNativeResource *p = ALLOC(PHPNativeResource);
	p->ce = ce;
	p->zobj = z_obj;
	VALUE resource = Data_Wrap_Struct(CLASS_OF(self), 0, php_native_resource_delete, p);
	rb_iv_set(self, "php_native_resource", resource);

	// define php instance properties and methods
	define_php_properties(self, ce, 0);
	define_php_methods(self, ce, 0);
	define_php_magic_method(self, ce, z_obj);

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
	return call_php_method_name_bridge(ce, zobj, callee, 0, NULL);
}

VALUE rb_php_object_setter(VALUE self, VALUE value)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	zval *zobj = get_zval(self);
	VALUE callee = get_callee_name();
	return call_php_method_name_bridge(ce, zobj, callee, 1, &value);
}

VALUE rb_php_object_call(int argc, VALUE *argv, VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	zval *zobj = get_zval(self);
	VALUE callee = get_callee_name();
	return call_php_method_name_bridge(ce, zobj, callee, argc, argv);
}

VALUE rb_php_object_call_magic_clone(VALUE self)
{
	TSRMLS_FETCH();

	zval *zobj = get_zval(self);
	zend_object_clone_obj_t handler = Z_OBJ_HT_P(zobj)->clone_obj;
	if (!handler) {
		rb_raise(rb_ePHPError, "clone_obj handler is not defined");
	}

	zval *retval;
	ALLOC_ZVAL(retval);
	Z_OBJVAL_P(retval) = handler(zobj TSRMLS_CC);
	Z_TYPE_P(retval) = IS_OBJECT;
	Z_SET_REFCOUNT_P(retval, 0);
	Z_SET_ISREF_P(retval);

	VALUE report = rb_cv_get(rb_mPHPVM, "@@last_error_reporting");
	rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
	if (g_exception) {
		VALUE exception = zval_to_value(g_exception);
		zval_ptr_dtor(&g_exception);
		g_exception = NULL;
		rb_exc_raise(exception);
	} else if (report!=Qnil) {
		rb_exc_raise(report);
	}

	return zval_to_value(retval);
}

VALUE rb_php_object_call_magic___get(VALUE self, VALUE name)
{
	TSRMLS_FETCH();

	zval *zobj = get_zval(self);
	zend_object_read_property_t handler = Z_OBJ_HT_P(zobj)->read_property;
	if (!handler) {
		rb_raise(rb_ePHPError, "read_property handler is not defined");
	}

	zval *member;
	MAKE_STD_ZVAL(member);
	ZVAL_STRING(member, RSTRING_PTR(name), 1);

	zval *retval;
	retval = handler(zobj, member, BP_VAR_IS, NULL TSRMLS_CC);

	zval_ptr_dtor(&member);

	VALUE report = rb_cv_get(rb_mPHPVM, "@@last_error_reporting");
	rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
	if (g_exception) {
		VALUE exception = zval_to_value(g_exception);
		zval_ptr_dtor(&g_exception);
		g_exception = NULL;
		rb_exc_raise(exception);
	} else if (report!=Qnil) {
		rb_exc_raise(report);
	}

	return zval_to_value(retval);
}

VALUE rb_php_object_call_magic___set(VALUE self, VALUE name, VALUE arg)
{
	TSRMLS_FETCH();

	zval *zobj = get_zval(self);
	zend_object_write_property_t handler = Z_OBJ_HT_P(zobj)->write_property;
	if (!handler) {
		rb_raise(rb_ePHPError, "write_property handler is not defined");
	}

	zval *member;
	MAKE_STD_ZVAL(member);
	ZVAL_STRING(member, RSTRING_PTR(name), 1);

	zval *z_arg;
	value_to_zval(arg, &z_arg);

	handler(zobj, member, z_arg, NULL TSRMLS_CC);

	zval_ptr_dtor(&member);
	zval_ptr_dtor(&z_arg);

	VALUE report = rb_cv_get(rb_mPHPVM, "@@last_error_reporting");
	rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
	if (g_exception) {
		VALUE exception = zval_to_value(g_exception);
		zval_ptr_dtor(&g_exception);
		g_exception = NULL;
		rb_exc_raise(exception);
	} else if (report!=Qnil) {
		rb_exc_raise(report);
	}

	return Qnil;
}

VALUE rb_php_object_call_magic___unset(VALUE self, VALUE name)
{
	TSRMLS_FETCH();

	zval *zobj = get_zval(self);
	zend_object_unset_property_t handler = Z_OBJ_HT_P(zobj)->unset_property;
	if (!handler) {
		rb_raise(rb_ePHPError, "unset_property handler is not defined");
	}

	zval *member;
	MAKE_STD_ZVAL(member);
	ZVAL_STRING(member, RSTRING_PTR(name), 1);

	handler(zobj, member, NULL TSRMLS_CC);

	zval_ptr_dtor(&member);

	VALUE report = rb_cv_get(rb_mPHPVM, "@@last_error_reporting");
	rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
	if (g_exception) {
		VALUE exception = zval_to_value(g_exception);
		zval_ptr_dtor(&g_exception);
		g_exception = NULL;
		rb_exc_raise(exception);
	} else if (report!=Qnil) {
		rb_exc_raise(report);
	}

	return Qnil;
}

VALUE rb_php_object_call_magic___isset(VALUE self, VALUE name)
{
	TSRMLS_FETCH();

	zval *zobj = get_zval(self);
	zend_object_has_property_t handler = Z_OBJ_HT_P(zobj)->has_property;
	if (!handler) {
		rb_raise(rb_ePHPError, "has_property handler is not defined");
	}

	zval *member;
	MAKE_STD_ZVAL(member);
	ZVAL_STRING(member, RSTRING_PTR(name), 1);

	int has;
	has = handler(zobj, member, 0, NULL TSRMLS_CC);

	zval_ptr_dtor(&member);

	VALUE report = rb_cv_get(rb_mPHPVM, "@@last_error_reporting");
	rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);
	if (g_exception) {
		VALUE exception = zval_to_value(g_exception);
		zval_ptr_dtor(&g_exception);
		g_exception = NULL;
		rb_exc_raise(exception);
	} else if (report!=Qnil) {
		rb_exc_raise(report);
	}

	return has ? Qtrue : Qfalse;
}

VALUE rb_php_object_call_magic___call(VALUE self, VALUE name, VALUE args)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	zval *zobj = get_zval(self);
	VALUE argv[2] = {name, args};

	if (args==Qnil || TYPE(args)!=T_ARRAY) {
		rb_raise(rb_eArgError, "args is not array");
	}

	return call_php_method_bridge(ce, zobj, ce->__call, 2, argv);
}

VALUE rb_php_object_call_magic___tostring(VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	zval *zobj = get_zval(self);

	return call_php_method_bridge(ce, zobj, ce->__tostring, 0, NULL);
}

VALUE rb_php_object_call_method_missing(int argc, VALUE *argv, VALUE self)
{
	zend_class_entry *ce = get_zend_class_entry(self);
	zval *zobj = get_zval(self);

	if (ce->__call) {
		// __call
		VALUE name, args;
		rb_scan_args(argc, argv, "1*", &name, &args);
		name = rb_str_new2(rb_id2name(SYM2ID(name)));
		VALUE argv2[2] = {name, args};

		return call_php_method_bridge(ce, zobj, ce->__call, 2, argv2);
	} else {
		// accessor
		VALUE name, val;
		rb_scan_args(argc, argv, "11", &name, &val);
		name = rb_str_new2(rb_id2name(SYM2ID(name)));

		VALUE is_setter = rb_funcall(name, rb_intern("end_with?"), 1, rb_str_new2("="));
		if (is_setter) {
			// __set
			rb_funcall(name, rb_intern("gsub!"), 2, rb_str_new2("="), rb_str_new2(""));
			return rb_php_object_call_magic___set(self, name, val);
		} else {
			// __get
			return rb_php_object_call_magic___get(self, name);
		}
	}
	return Qnil;
}


// class PHPVM::PHPExceptionObject

VALUE rb_php_exception_object_initialize(int argc, VALUE *argv, VALUE self)
{
	rb_call_super(argc, argv);
	return self;
}


// class PHPVM::PHPErrorReporting

VALUE rb_php_error_reporting_initialize(int argc, VALUE *argv, VALUE self)
{
	VALUE log_message = Qnil;
	VALUE error_level = Qnil;
	VALUE message = Qnil;
	VALUE file = Qnil;
	VALUE line = Qnil;

	if (argc==1 && TYPE(argv[0])==T_STRING) {
		log_message = argv[0];
		VALUE re_str = rb_str_new2("^(?:PHP )?([^:]+?)(?: error)?: {0,2}(.+) in (.+) on line (\\d+)$");
		VALUE re_option = rb_const_get(rb_cRegexp, rb_intern("MULTILINE"));
		VALUE report_re = rb_funcall(rb_cRegexp, rb_intern("new"), 2, re_str, re_option);
		VALUE m = rb_funcall(argv[0], rb_intern("match"), 1, report_re);
		if (m!=Qnil) {
			error_level = rb_funcall(m, rb_intern("[]"), 1, INT2NUM(1));
			error_level = ID2SYM(rb_intern(RSTRING_PTR(error_level)));
			message = rb_funcall(m, rb_intern("[]"), 1, INT2NUM(2));
			file = rb_funcall(m, rb_intern("[]"), 1, INT2NUM(3));
			line = rb_funcall(m, rb_intern("[]"), 1, INT2NUM(4));
			line = rb_funcall(line, rb_intern("to_i"), 0);
			argv[0] = message;
		}
	}
	rb_call_super(argc, argv);

	rb_iv_set(self, "log_message", log_message);
	rb_iv_set(self, "error_level", error_level);
	rb_iv_set(self, "file", file);
	rb_iv_set(self, "line", line);

	return self;
}

VALUE rb_php_error_reporting_log_message(VALUE self)
{
	return rb_iv_get(self, "log_message");
}

VALUE rb_php_error_reporting_error_level(VALUE self)
{
	return rb_iv_get(self, "error_level");
}

VALUE rb_php_error_reporting_file(VALUE self)
{
	return rb_iv_get(self, "file");
}

VALUE rb_php_error_reporting_line(VALUE self)
{
	return rb_iv_get(self, "line");
}


// module

void php_vm_module_exit()
{
	TSRMLS_FETCH();
	php_embed_shutdown(TSRMLS_C);
}

void Init_php_vm()
{
#ifdef ZTS
	void ***tsrm_ls;
#endif

	// set php_embed callback function
	php_embed_module.ub_write = php_embed_output_handler;
	php_embed_module.log_message = php_embed_error_handler;

	// initialize php_embed
	int init_argc = 1;
	char *init_argv[2] = {"php_vm", NULL};
	php_embed_init(init_argc, init_argv PTSRMLS_CC);
	EG(bailout) = NULL;

	// set exit php_vm callback function
	atexit(php_vm_module_exit);

	// set php_embed hook function
	zend_throw_exception_hook = php_vm_exception_hook;

	// ini
	zend_try {
		//zend_alter_ini_entry("display_errors", sizeof("display_errors"), "1", sizeof("0")-1, PHP_INI_SYSTEM, PHP_INI_STAGE_RUNTIME);
		zend_alter_ini_entry("log_errors", sizeof("log_errors"), "1", sizeof("1")-1, PHP_INI_SYSTEM, PHP_INI_STAGE_RUNTIME);
	} zend_catch {
	} zend_end_try();

	// module PHPVM
	rb_mPHPVM = rb_define_module("PHPVM");

	rb_define_singleton_method(rb_mPHPVM, "output_handler", rb_php_vm_get_output_handler, 0);
	rb_define_singleton_method(rb_mPHPVM, "output_handler=", rb_php_vm_set_output_handler, 1);
	rb_define_singleton_method(rb_mPHPVM, "error_handler", rb_php_vm_get_error_handler, 0);
	rb_define_singleton_method(rb_mPHPVM, "error_handler=", rb_php_vm_set_error_handler, 1);

	rb_define_singleton_method(rb_mPHPVM, "require", rb_php_vm_require, 1);
	rb_define_singleton_method(rb_mPHPVM, "require_once", rb_php_vm_require_once, 1);
	rb_define_singleton_method(rb_mPHPVM, "include", rb_php_vm_include, 1);
	rb_define_singleton_method(rb_mPHPVM, "include_once", rb_php_vm_include_once, 1);

	rb_define_singleton_method(rb_mPHPVM, "exec", rb_php_vm_exec, 1);
	rb_define_singleton_method(rb_mPHPVM, "get_class", rb_php_vm_get_class, 1);
	rb_define_singleton_method(rb_mPHPVM, "define_global", rb_php_vm_define_global, 0);

	rb_define_const(rb_mPHPVM, "VERSION", rb_str_new2("1.3.11"));

	rb_cv_set(rb_mPHPVM, "@@output_handler", Qnil);
	rb_cv_set(rb_mPHPVM, "@@error_handler", Qnil);
	rb_cv_set(rb_mPHPVM, "@@last_error_reporting", Qnil);

	// module PHPVM::PHPGlobal
	rb_mPHPGlobal = rb_define_module_under(rb_mPHPVM, "PHPGlobal");

	rb_define_module_function(rb_mPHPGlobal, "require", rb_php_global_require, 1);
	rb_define_module_function(rb_mPHPGlobal, "require_once", rb_php_global_require_once, 1);
	rb_define_module_function(rb_mPHPGlobal, "echo", rb_php_global_echo, -1);
	rb_define_module_function(rb_mPHPGlobal, "print", rb_php_global_print, 1);
	rb_define_module_function(rb_mPHPGlobal, "array", rb_php_global_array, -1);

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

	// class PHPVM::PHPErrorReporting < PHPVM::PHPError
	rb_ePHPErrorReporting = rb_define_class_under(rb_mPHPVM, "PHPErrorReporting", rb_ePHPError);
	rb_define_method(rb_ePHPErrorReporting, "initialize", rb_php_error_reporting_initialize, -1);
	rb_define_method(rb_ePHPErrorReporting, "log_message", rb_php_error_reporting_log_message, 0);
	rb_define_method(rb_ePHPErrorReporting, "error_level", rb_php_error_reporting_error_level, 0);
	rb_define_method(rb_ePHPErrorReporting, "file", rb_php_error_reporting_file, 0);
	rb_define_method(rb_ePHPErrorReporting, "line", rb_php_error_reporting_line, 0);

	rb_php_vm_define_global(rb_mPHPVM);
}
