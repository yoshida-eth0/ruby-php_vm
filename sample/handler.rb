#!/usr/bin/ruby


$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

PHPVM.output_handler = Proc.new {|output|
  puts "output: #{output}"
}
PHPVM.error_handler = Proc.new {|error_report|
  #puts "error: #{error_report.log_message}"
  puts "error: #{error_report.error_level}: #{error_report.message} in #{error_report.file} on line #{error_report.line}"
}

begin
  PHPVM.exec <<-EOS
    echo "hello\n";
    trigger_error("user notice", E_USER_NOTICE);
    trigger_error("user warning", E_USER_WARNING);
    trigger_error("user error", E_USER_ERROR);
  EOS
rescue PHPVM::PHPErrorReporting => e
  puts "---"
  puts "PHPErrorReporting occurred"
  puts "#{e.error_level}: #{e.message}"
end
