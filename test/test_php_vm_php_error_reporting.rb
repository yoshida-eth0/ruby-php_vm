#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "test/unit"
require "php_vm"

class TC_PHPVM_PHPErrorReporting < Test::Unit::TestCase
  def setup
    PHPVM.output_handler = Proc.new{|output| }
    PHPVM.error_handler = Proc.new{|error_reporting| }
    PHPVM.error_handler = nil
  end

  def test_raise_exception
    assert_raises(PHPVM::PHPErrorReporting) {
      PHPVM.exec("trigger_error('fatal err', E_USER_ERROR);")
    }

    assert_raises(PHPVM::PHPErrorReporting) {
      PHPVM::PHPGlobal.trigger_error("fatal err", PHPVM::PHPGlobal::E_USER_ERROR)
    }
  end
end
