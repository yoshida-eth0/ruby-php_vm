#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

begin
  PHPVM.exec("throw new Exception('throw exception!!');")
rescue PHPVM::PHPError => e
  puts "#{e.php_class.name}: #{e}"
end
