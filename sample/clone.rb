#!/usr/bin/ruby


$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

PHPVM.exec <<EOS
class SubObject
{
    static $instances = 0;
    public $instance;

    public function __construct() {
        $this->instance = ++self::$instances;
    }

    public function __clone() {
        $this->instance = ++self::$instances;
    }
}

class MyCloneable
{
    public $object1;
    public $object2;

    function __clone()
    {
        // Force a copy of this->object, otherwise
        // it will point to same object.
        $this->object1 = clone $this->object1;
    }
}
EOS

MyCloneable = PHPVM.get_class("MyCloneable")
SubObject = PHPVM.get_class("SubObject")

obj = MyCloneable.new
obj.object1 = SubObject.new
obj.object2 = SubObject.new

obj2 = obj.clone

puts "Original Object:"
PHPVM::PHPGlobal.print_r(obj)

puts "Cloned Object:"
PHPVM::PHPGlobal.print_r(obj2)
