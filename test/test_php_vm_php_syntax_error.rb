#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "test/unit"
require "php_vm"

class TC_PHPVM_PHPSyntaxError < Test::Unit::TestCase
  def setup
    PHPVM.output_handler = Proc.new{|output| }
    PHPVM.error_handler = Proc.new{|error_reporting| }
  end

  def test_raise_exception
    assert_raises(PHPVM::PHPSyntaxError) {
      PHPVM.exec("invalid syntax code")
    }
  end
end
