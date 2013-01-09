#!/usr/bin/ruby

require 'test/unit'

test_dir = File.dirname(__FILE__)
exit Test::Unit::AutoRunner.run(true, test_dir)
