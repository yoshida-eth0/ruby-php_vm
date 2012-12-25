#!/usr/bin/ruby


$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

class PHPWorld
  include PHPVM::PHPGlobal

  def initialize
    url_str = "http://maps.google.com/maps?q=Shinjuku,+Tokyo,+Japan&hl=en&ll=35.693831,139.703522&spn=0.117805,0.110893&sll=37.0625,-95.677068&sspn=57.945758,56.777344&oq=shinjuku&hnear=Shinjuku,+Tokyo,+Japan&t=m&z=13"
    url_hash = parse_url(url_str)
    print_r(url_hash)
  end
end

PHPWorld.new
