#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "test/unit"
require "php_vm"

class TC_PHPVM_PHPObject < Test::Unit::TestCase
  def setup
    PHPVM.output_handler = Proc.new{|output| }
    PHPVM.error_handler = Proc.new{|error_reporting| }

    # define MagicAccessor
    begin
      PHPVM.get_class("MagicAccessor")
    rescue => e
      PHPVM.exec <<-EOS
        class MagicAccessor
        {
          protected $_data = array();

          public function __get($name)
          {
            return isset($this->_data[$name]) ? $this->_data[$name] : null;
          }

          public function __set($name, $val)
          {
            $this->_data[$name] = $val;
          }
        }
      EOS
    end

    # define MagicCaller
    begin
      PHPVM.get_class("MagicCaller")
    rescue => e
      PHPVM.exec <<-EOS
        class MagicCaller
        {
          public function __call($name, $args)
          {
            return $name." ".implode(", ", $args);
          }
        }
      EOS
    end
  end

  def test_php_class
    phpSimpleXMLElement = PHPVM.get_class("SimpleXMLElement")
    xml = phpSimpleXMLElement.new("<root><item>Hello World!!</item></root>")

    assert_equal(phpSimpleXMLElement, xml.php_class)
  end

  def test_call
    phpException = PHPVM.get_class("Exception")
    exc = phpException.new("exc message", 123)

    assert_equal("exc message", exc.getMessage)
    assert_equal(123, exc.getCode)
  end

  def test_magic_access
    # default object handler
    phpMagicAccessor = PHPVM.get_class("MagicAccessor")
    acc = phpMagicAccessor.new
    acc.aaa = "AAA"

    assert_equal("AAA", acc.aaa)
    assert_nil(acc.bbb)

    # custom object handler
    phpSimpleXMLElement = PHPVM.get_class("SimpleXMLElement")
    xml = phpSimpleXMLElement.new("<root><item>Hello World!!</item></root>")

    assert_equal("Hello World!!", xml.item.to_s)
  end

  def test_magic_call
    phpMagicCaller = PHPVM.get_class("MagicCaller")
    cal = phpMagicCaller.new

    assert_equal("hello aaa, bbb, ccc", cal.hello("aaa", "bbb", "ccc"))
  end
end
