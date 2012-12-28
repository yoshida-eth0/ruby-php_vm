#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

class RequireClass
  include PHPVM::PHPGlobal

  def initialize
    echo "require php\n"
    require File.expand_path(__FILE__+"/../require.php")
    echo "\n"

    echo "require ruby\n"
    begin
      require "active_record"
      echo ActiveRecord
    rescue PHPVM::PHPError, LoadError => e
      echo e.to_s
    end
    echo "\n"
  end
end

RequireClass.new
