# -*- encoding: utf-8 -*-
require File.expand_path('../lib/php_vm/version', __FILE__)

Gem::Specification.new do |gem|
  gem.authors       = ["Yoshida Tetsuya"]
  gem.email         = ["yoshida.eth0@gmail.com"]
  gem.description   = %q{php_vm is a native bridge between Ruby and PHP.}
  gem.summary       = %q{php_vm is a native bridge between Ruby and PHP.}
  gem.homepage      = %q{https://github.com/yoshida-eth0/ruby-php_vm}

  gem.files         = `git ls-files`.split($\).delete_if{|x| x.match(/^sample/)}
  gem.executables   = gem.files.grep(%r{^bin/}).map{ |f| File.basename(f) }
  gem.test_files    = gem.files.grep(%r{^(test|spec|features)/})
  gem.name          = "php_vm"
  gem.require_paths = ["lib"]
  gem.extensions    = ["ext/extconf.rb"]
  gem.version       = PHPVM::VERSION
end
