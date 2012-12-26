#include "php_vm.h"
#include "php_vm_z2v.h"
#include <string.h>


static int is_array_convertable(HashTable* ht)
{
	HashPosition pos;
	char  *string_key;
	ulong num_index;
	ulong index = 0;

	zend_hash_internal_pointer_reset_ex(ht, &pos);
	do {
		switch(zend_hash_get_current_key_ex(ht, &string_key, NULL, &num_index, 0, &pos)) {
			case HASH_KEY_IS_STRING:
				return 0;
			case HASH_KEY_NON_EXISTANT:
				return 1;
			case HASH_KEY_IS_LONG:
				if (num_index != index) {
					return 0;
				}
				++index;
		}
	} while(SUCCESS == zend_hash_move_forward_ex(ht, &pos));
	return 1;
}

static VALUE zval_to_value_array(HashTable* ht)
{
	HashPosition pos;
	zval** data;
	VALUE ret;

	ret = rb_ary_new2(zend_hash_num_elements(ht));

	zend_hash_internal_pointer_reset_ex(ht, &pos);
	while (SUCCESS == zend_hash_get_current_data_ex(ht, (void **)&data, &pos)) {
		VALUE t = zval_to_value(*data);
		rb_ary_push(ret, t);
		zend_hash_move_forward_ex(ht, &pos);
	}
	return ret;
}

static VALUE zval_to_value_hash(HashTable* ht)
{
	HashPosition pos;
	zval** data;
	VALUE ret;

	ret = rb_hash_new();

	zend_hash_internal_pointer_reset_ex(ht, &pos);
	while (SUCCESS == zend_hash_get_current_data_ex(ht, (void **)&data, &pos)) {
		char* string_key;
		ulong num_index;
		VALUE key = Qnil;
		VALUE val = zval_to_value(*data);

		switch(zend_hash_get_current_key_ex(ht, &string_key, NULL, &num_index, 0, &pos)) {
			case HASH_KEY_IS_STRING:
				key = rb_str_new2(string_key);
				break;
			case HASH_KEY_IS_LONG:
				key = LONG2NUM(num_index);
				break;
		}

		rb_hash_aset(ret, key, val);
		zend_hash_move_forward_ex(ht, &pos);
	}
	return ret;
}

static VALUE zval_to_value_object(zval *z)
{
	TSRMLS_FETCH();

	// class name
	const char *name = "";
	zend_uint name_len = 0;
	int dup;
	dup = zend_get_object_classname(z, &name, &name_len TSRMLS_CC);
	VALUE v_name = rb_str_new(name, name_len);

	// class
	VALUE class = rb_php_class_get(rb_cPHPClass, v_name);

	// object
	VALUE obj = Qnil;
	if (is_exception_zval(z)) {
		// exception object
		zval *z_message = zend_read_property(zend_exception_get_default(TSRMLS_C), z, "message", sizeof("message")-1, 0 TSRMLS_CC);

		obj = rb_exc_new2(rb_ePHPExceptionObject, Z_STRVAL_P(z_message));

		rb_iv_set(obj, "php_class", class);
	} else {
		// normal object
		obj = rb_obj_alloc(rb_cPHPObject);

		rb_iv_set(obj, "php_class", class);
	}

	// retain
	Z_ADDREF_P(z);

	// resource
	PHPNativeResource *p = ALLOC(PHPNativeResource);
	p->ce = get_zend_class_entry(class);
	p->zobj = z;
	VALUE resource = Data_Wrap_Struct(CLASS_OF(obj), 0, php_native_resource_delete, p);
	rb_iv_set(obj, "php_native_resource", resource);

	return obj;
}


VALUE zval_to_value(zval *z)
{
	if (z) {
		switch(Z_TYPE_P(z)) {
			case IS_NULL:
				return Qnil;
			case IS_BOOL:
				return (zval_is_true(z)) ? Qtrue : Qfalse;
			case IS_LONG:
				return INT2NUM(Z_LVAL_P(z));
			case IS_DOUBLE:
				return rb_float_new(Z_DVAL_P(z));
			case IS_ARRAY:
			case IS_CONSTANT_ARRAY:{
				HashTable* ht = Z_ARRVAL_P(z);

				if (0 == zend_hash_num_elements(ht)) {
					return rb_ary_new();
				}

				if (is_array_convertable(ht)) {
					return zval_to_value_array(ht);
				}

				return zval_to_value_hash(ht);
			}
			case IS_OBJECT:
			case IS_RESOURCE:
			case IS_CONSTANT:
				return zval_to_value_object(z);
			case IS_STRING:
				return rb_str_new(Z_STRVAL_P(z), Z_STRLEN_P(z));
			default:
				return Qnil;
		}
	}
	return Qnil;
}


