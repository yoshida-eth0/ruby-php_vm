#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "test/unit"
require "php_vm"

class TC_PHPVM < Test::Unit::TestCase
  def setup
    PHPVM.output_handler = Proc.new{|output| }
    PHPVM.error_handler = Proc.new{|error_reporting| }
  end

  def test_output_handler_accessor
    hooked_output = nil
    pr = Proc.new {|output|
      hooked_output = output
    }

    # proc object
    PHPVM.output_handler = pr
    assert_equal(pr, PHPVM.output_handler)

    # hook
    PHPVM.exec("echo 'HelloWorld';")
    assert_equal("HelloWorld", hooked_output)

    # nil object
    PHPVM.output_handler = nil
    assert_nil(PHPVM.output_handler)
  end

  def test_error_handler_accessor
    hooked_error_reporting = nil
    pr = Proc.new {|error_reporting|
      hooked_error_reporting = error_reporting
    }

    # proc object
    PHPVM.error_handler = pr
    assert_equal(pr, PHPVM.error_handler)

    # hook
    PHPVM.exec("strpos();")
    assert_instance_of(PHPVM::PHPErrorReporting, hooked_error_reporting)

    # nil object
    PHPVM.error_handler = nil
    assert_nil(PHPVM.error_handler)
  end

  def test_require
    # exists php file
    filepath = File.expand_path(__FILE__+"/../require.php")
    assert_equal(true, PHPVM.require(filepath))

    # non exists php file
    assert_raises(PHPVM::PHPErrorReporting) {
      filepath = File.expand_path(__FILE__+"/../non_exists_file.php")
      PHPVM.require(filepath)
    }
  end

  def test_require_once
    # exists php file
    filepath = File.expand_path(__FILE__+"/../require.php")
    assert_equal(true, PHPVM.require_once(filepath))

    # non exists php file
    assert_raises(PHPVM::PHPErrorReporting) {
      filepath = File.expand_path(__FILE__+"/../non_exists_file.php")
      PHPVM.require_once(filepath)
    }
  end

  def test_include
    # exists php file
    filepath = File.expand_path(__FILE__+"/../require.php")
    assert_equal(true, PHPVM.include(filepath))

    # non exists php file
    filepath = File.expand_path(__FILE__+"/../non_exists_file.php")
    assert_equal(false, PHPVM.include(filepath))
  end

  def test_include_once
    # exists php file
    filepath = File.expand_path(__FILE__+"/../require.php")
    assert_equal(true, PHPVM.include_once(filepath))

    # non exists php file
    filepath = File.expand_path(__FILE__+"/../non_exists_file.php")
    assert_equal(false, PHPVM.include_once(filepath))
  end

  def test_get_class
    # exists php class
    assert_instance_of(PHPVM::PHPClass, PHPVM.get_class("SimpleXMLElement"))
    assert_equal(PHPVM::PHPClass.get("SimpleXMLElement"), PHPVM.get_class("SimpleXMLElement"))

    # non exists php class
    assert_raises(PHPVM::PHPError) {
      PHPVM.get_class("NonExistsClass")
    }
  end

  def test_define_global
    assert_equal(true, PHPVM.define_global)
  end

  def test_version
    assert_match(/^\d+\.\d+\.\d+$/, PHPVM::VERSION)
  end
end
