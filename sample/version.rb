#!/usr/bin/ruby

$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/")
require "php_vm"

puts PHPVM::VERSION
