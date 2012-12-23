#!/usr/bin/ruby

require_relative File.expand_path(__FILE__+"/../../ext/php_vm")

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

	public static function classGetHello($name)
	{
		return "Hello {$name}!!";
	}

	public static function classSayHello($name)
	{
		var_dump(self::classGetHello($name));
	}
}
EOS

HelloClass = PHPVM::PHPClass.get("HelloClass")

puts "[class]"
puts HelloClass
puts HelloClass.name
puts ""

puts "[instance method]"
h = HelloClass.new("instance world")
h.instanceSayHello
p h.instanceGetHello
puts ""

puts "[class method]"
HelloClass.classSayHello("class world")
p HelloClass.classGetHello("class world")
