#!/usr/bin/ruby


$LOAD_PATH << File.expand_path(__FILE__+"/../../ext/php_vm")
require "php_vm"

PHPVM.exec <<EOS
class MissingAccessor
{
	protected $data = array();

	public function __get($name)
	{
		return isset($this->data[$name]) ? $this->data[$name] : null;
	}

	public function __set($name, $val)
	{
		$this->data[$name] = $val;
	}
}
EOS

MissingAccessor = PHPVM.get_class("MissingAccessor")
ma = MissingAccessor.new
ma.aaa = "a val"
ma.bbb = "b val"
ma.ccc = "c val"
p ma.aaa
p ma.bbb
p ma.ccc
