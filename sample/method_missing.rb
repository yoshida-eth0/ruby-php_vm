#!/usr/bin/ruby


$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

PHPVM.exec <<EOS
class MissingTest
{
	public function __call($name, $args)
	{
		echo "__call: $name ";
		print_r($args);
	}

	public static function __callStatic($name, $args)
	{
		echo "__callStatic: $name ";
		print_r($args);
	}
}
EOS

MissingTest = PHPVM.get_class("MissingTest")
MissingTest.new.instance_missing_method("hello", "world")
MissingTest.class_missing_method("Hello", "World")
