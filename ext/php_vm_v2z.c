#include "php_vm.h"
#include "php_vm_v2z.h"
#include <string.h>


static void value_to_zval_array(VALUE v, zval *z)
{
	array_init(z);

	long i;
	for (i=0; i<RARRAY_LEN(v); i++) {
		zval *new_var;
		MAKE_STD_ZVAL(z);
		value_to_zval(RARRAY_PTR(v)[i], new_var);

		zend_hash_next_index_insert(Z_ARRVAL_P(z), &new_var, sizeof(zval *), NULL);
	}
}

static void value_to_zval_hash(VALUE v, zval *z)
{
	array_init(z);

	v = rb_funcall(v, rb_intern("flatten"), 0);

	long i;
	for (i=0; i<RARRAY_LEN(v); i+=2) {
		VALUE v_key = RARRAY_PTR(v)[i];
		StringValue(v_key);

		zval *z_value;
		MAKE_STD_ZVAL(z);
		value_to_zval(RARRAY_PTR(v)[i+1], z_value);

		add_assoc_zval_ex(z, RSTRING_PTR(v_key), RSTRING_LEN(v), z_value);
	}
}


void value_to_zval(VALUE v, zval *z)
{
	switch (TYPE(v)) {
		// nil
		case T_NIL:
printf("value_to_zval nil\n");
			ZVAL_NULL(z);
			break;
		// bool
		case T_TRUE:
printf("value_to_zval true\n");
			ZVAL_TRUE(z);
			break;
		case T_FALSE:
printf("value_to_zval false\n");
			ZVAL_FALSE(z);
			break;
		// number
		case T_FIXNUM:
printf("value_to_zval fixnum\n");
			ZVAL_LONG(z, NUM2LONG(v));
			break;
		case T_FLOAT:
printf("value_to_zval float\n");
			ZVAL_DOUBLE(z, RFLOAT_VALUE(v));
			break;
		// array
		case T_ARRAY:
printf("value_to_zval array\n");
			value_to_zval_array(v, z);
			break;
		// hash
		case T_HASH:
printf("value_to_zval hash\n");
			value_to_zval_hash(v, z);
			break;
		// object string
		default:{
			VALUE cls = CLASS_OF(v);
			if (cls==rb_cPHPObject || cls==rb_ePHPExceptionObject) {
				// wrap php object
printf("value_to_zval php object\n");
			} else {
				// other to_s
printf("value_to_zval other to_s\n");
				StringValue(v);
				ZVAL_STRING(z, RSTRING_PTR(v), 1);
			}
		}
	}
}
