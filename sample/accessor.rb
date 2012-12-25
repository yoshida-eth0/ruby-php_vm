#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

PHPVM.exec <<EOS
class AccessorClass
{
	// instance

	public $ivar1;
	public $ivar2;
	public $ivar3;

	public function instance_dump()
	{
		var_dump(get_object_vars($this));
	}


	// static

	public static $svar1;
	public static $svar2;
	public static $svar3;

	public static function static_dump()
	{
		var_dump(get_class_vars(__CLASS__));
	}
}
EOS

AccessorClass = PHPVM.get_class("AccessorClass")

ac = AccessorClass.new
ac.ivar1 = "abc"
ac.ivar2 = "def"
ac.ivar3 = "ghi"
p ac.ivar1
p ac.ivar2
p ac.ivar3
ac.instance_dump
puts ""

AccessorClass.svar1 = "ABC"
AccessorClass.svar2 = "DEF"
AccessorClass.svar3 = "GHI"
p AccessorClass.svar1
p AccessorClass.svar2
p AccessorClass.svar3
AccessorClass.static_dump
