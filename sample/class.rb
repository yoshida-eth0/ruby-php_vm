#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

PHPVM.exec <<EOS
class HelloClass
{
	public function __construct($name)
	{
		$this->name = $name;
	}


	// instance

	public function instanceGetHello()
	{
		return "Hello {$this->name}!!";
	}

	public function instanceSayHello()
	{
		var_dump($this->instanceGetHello());
	}


	// static

	public static function staticGetHello($name)
	{
		return "Hello {$name}!!";
	}

	public static function staticSayHello($name)
	{
		var_dump(self::staticGetHello($name));
	}
}
EOS

HelloClass = PHPVM::getClass("HelloClass")

puts "[class]"
puts HelloClass
puts HelloClass.name
puts ""

puts "[instance method]"
h = HelloClass.new("instance world")
h.instanceSayHello
p h.instanceGetHello
puts ""

puts "[static method]"
HelloClass.staticSayHello("static world")
p HelloClass.staticGetHello("static world")
