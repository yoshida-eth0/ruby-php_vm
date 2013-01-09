#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "test/unit"
require "php_vm"

class TC_PHPVM_PHPClass < Test::Unit::TestCase
  def setup
    PHPVM.output_handler = Proc.new{|output| }
    PHPVM.error_handler = Proc.new{|error_reporting| }
  end

  def test_get
    # exists php class
    assert_instance_of(PHPVM::PHPClass, PHPVM::PHPClass.get("SimpleXMLElement"))

    # non exists php class
    assert_raises(PHPVM::PHPError) {
      PHPVM.get_class("NonExistsClass")
    }
  end

  def test_new
    # normal object
    phpSimpleXMLElement = PHPVM.get_class("SimpleXMLElement")
    xml = phpSimpleXMLElement.new("<root><item>Hello World!!</item></root>")
    assert_instance_of(PHPVM::PHPObject, xml)

    # exception object
    phpException = PHPVM.get_class("Exception")
    exc = phpException.new("php error")
    assert_instance_of(PHPVM::PHPExceptionObject, exc)
  end

  def test_name
    phpSimpleXMLElement = PHPVM.get_class("SimpleXMLElement")
    assert_equal("SimpleXMLElement", phpSimpleXMLElement.name)
  end
end
