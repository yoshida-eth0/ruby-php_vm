#!/usr/bin/ruby


$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

PHPVM.exec <<EOS
class JsonConvert
{
	public static function convert($obj)
	{
		return json_decode(json_encode($obj), 1);
	}
}
EOS

JsonConvert = PHPVM.getClass("JsonConvert")

h = {
  :arr => [:a, :b, :c],
  :hash => {
    :k1 => :v1,
    "k2" => "v2",
  },
  :str => "strvalue",
  :sym => :symvalue,
}
p h
p JsonConvert.convert(h)
