#!/usr/bin/ruby

require_relative File.expand_path(__FILE__+"/../../ext/php_vm")

PHPVM.exec('echo "Hello world!!\n";')
