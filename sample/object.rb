#!/usr/bin/ruby


$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

PHPVM.exec <<EOS
class ObjectRef
{
	protected $_val = null;

	public function __construct($val)
	{
		$this->_val = $val;
	}

	public static function create($val)
	{
		return new self($val);
	}

	public static function show($obj)
	{
		var_dump($obj);
	}
}
EOS

ObjectRef = PHPVM.getClass("ObjectRef")

o = ObjectRef.create("hello")
ObjectRef.show(o)
