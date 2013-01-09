#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "test/unit"
require "php_vm"

class TC_PHPVM_PHPGlobal < Test::Unit::TestCase
  def setup
    PHPVM.output_handler = Proc.new{|output| }
    PHPVM.error_handler = Proc.new{|error_reporting| }
  end

  def test_require
    # exists php file
    filepath = File.expand_path(__FILE__+"/../require.php")
    assert_equal(true, PHPVM::PHPGlobal.require(filepath))

    # exists ruby file
    # return false if require already imported file.
    filepath = "rubygems"
    assert_equal(false, PHPVM::PHPGlobal.require(filepath))

    # non exists file
    assert_raises(LoadError) {
      filepath = File.expand_path(__FILE__+"/../non_exists_file.php")
      PHPVM::PHPGlobal.require(filepath)
    }
  end

  def test_require_once
    # exists php file
    filepath = File.expand_path(__FILE__+"/../require.php")
    assert_equal(true, PHPVM::PHPGlobal.require_once(filepath))

    # exists ruby file
    # return false if require already imported file.
    filepath = "rubygems"
    assert_equal(false, PHPVM::PHPGlobal.require(filepath))

    # non exists file
    assert_raises(LoadError) {
      filepath = File.expand_path(__FILE__+"/../non_exists_file.php")
      PHPVM::PHPGlobal.require_once(filepath)
    }
  end

  def test_echo
    hooked_output = nil
    PHPVM.output_handler = Proc.new{|output|
      hooked_output = output
    }

    # string
    PHPVM::PHPGlobal.echo("abcdefg")
    assert_equal("abcdefg", hooked_output)

    # array
    PHPVM::PHPGlobal.echo([])
    assert_equal("Array", hooked_output)
  end

  def test_print
    hooked_output = nil
    PHPVM.output_handler = Proc.new{|output|
      hooked_output = output
    }

    # string
    PHPVM::PHPGlobal.print("abcdefg")
    assert_equal("abcdefg", hooked_output)

    # array
    PHPVM::PHPGlobal.print([])
    assert_equal("Array", hooked_output)
  end

  def test_array
    assert_equal([], PHPVM::PHPGlobal.array())
    assert_equal([1, 2, 3], PHPVM::PHPGlobal.array(1, 2, 3))
    assert_equal({"aaa"=>"bbb"}, PHPVM::PHPGlobal.array("aaa"=>"bbb"))
  end

  def test_native_function
    # http_build_query
    assert_equal("aaa=bbb", PHPVM::PHPGlobal.http_build_query({"aaa"=>"bbb"}))

    # preg_replace
    assert_equal("aBcdef", PHPVM::PHPGlobal.preg_replace('/b/', "B", "abcdef"))

    # number_format
    assert_equal("123,456,789", PHPVM::PHPGlobal.number_format(123456789))
  end

  def test_native_class
    # stdClass
    assert_instance_of(PHPVM::PHPClass, PHPVM::PHPGlobal::stdClass)
    assert_equal(PHPVM::PHPClass.get("stdClass"), PHPVM::PHPGlobal::stdClass)

    # SimpleXMLElement
    assert_instance_of(PHPVM::PHPClass, PHPVM::PHPGlobal::SimpleXMLElement)
    assert_equal(PHPVM::PHPClass.get("SimpleXMLElement"), PHPVM::PHPGlobal::SimpleXMLElement)

    xml = PHPVM::PHPGlobal::SimpleXMLElement.new("<root><item>Hello World!!</item></root>")
    assert_equal("Hello World!!", xml.item.to_s)
  end

  def test_native_constant
    # PHP_VERSION
    assert_match(/^\d+\.\d+\.\d+$/, PHPVM::PHPGlobal::PHP_VERSION)
  end
end
