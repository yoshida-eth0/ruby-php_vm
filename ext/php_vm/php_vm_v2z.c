#include "php_vm.h"
#include "php_vm_v2z.h"
#include <string.h>


static void value_to_zval_array(VALUE v, zval *z)
{
	array_init(z);

	long i;
	for (i=0; i<RARRAY_LEN(v); i++) {
		zval *new_var;
		value_to_zval(RARRAY_PTR(v)[i], &new_var);

		zend_hash_next_index_insert(Z_ARRVAL_P(z), &new_var, sizeof(zval *), NULL);
	}
}

static int hash_flatten_yield(VALUE key, VALUE value, VALUE ary)
{
	rb_ary_push(ary, key);
	rb_ary_push(ary, value);
	return 0;
}

static void value_to_zval_hash(VALUE v, zval *z)
{
	VALUE v_arr = rb_ary_new();
	rb_hash_foreach(v, hash_flatten_yield, v_arr);

	array_init(z);

	long i;
	for (i=0; i<RARRAY_LEN(v_arr); i+=2) {
		VALUE v_key = RARRAY_PTR(v_arr)[i];
		v_key = rb_obj_as_string(v_key);

		zval *z_value;
		value_to_zval(RARRAY_PTR(v_arr)[i+1], &z_value);

		add_assoc_zval_ex(z, RSTRING_PTR(v_key), RSTRING_LEN(v_key)+1, z_value);
	}
}


void value_to_zval(VALUE v, zval **z)
{
	switch (TYPE(v)) {
		// nil
		case T_NIL:
			MAKE_STD_ZVAL(*z);
			ZVAL_NULL(*z);
			break;
		// bool
		case T_TRUE:
			MAKE_STD_ZVAL(*z);
			ZVAL_TRUE(*z);
			break;
		case T_FALSE:
			MAKE_STD_ZVAL(*z);
			ZVAL_FALSE(*z);
			break;
		// number
		case T_FIXNUM:
			MAKE_STD_ZVAL(*z);
			ZVAL_LONG(*z, NUM2LONG(v));
			break;
		case T_FLOAT:
			MAKE_STD_ZVAL(*z);
			ZVAL_DOUBLE(*z, RFLOAT_VALUE(v));
			break;
		// array
		case T_ARRAY:
			MAKE_STD_ZVAL(*z);
			value_to_zval_array(v, *z);
			break;
		// hash
		case T_HASH:
			MAKE_STD_ZVAL(*z);
			value_to_zval_hash(v, *z);
			break;
		// object string
		default:{
			zval *resource_zobj = get_zval(v);
			if (resource_zobj) {
				// php native resource
				*z = resource_zobj;
				Z_ADDREF_PP(z);
			} else {
				// other to_s
				v = rb_obj_as_string(v);
				MAKE_STD_ZVAL(*z);
				ZVAL_STRING(*z, RSTRING_PTR(v), 1);
			}
		}
	}
}
