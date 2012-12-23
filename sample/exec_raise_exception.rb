#!/usr/bin/ruby

require_relative File.expand_path(__FILE__+"/../../ext/php_vm")

begin
  PHPVM.exec("throw new Exception('throw exception!!');")
rescue PHPVM::PHPError => e
  puts "#{e.php_class.name}: #{e}"
end
