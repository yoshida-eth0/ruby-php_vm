# -*- encoding: utf-8 -*-

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
  gem.extensions    = ["ext/php_vm/extconf.rb"]
  gem.version       = Proc.new {
                        version = nil
                        txt = open(File.expand_path("../ext/php_vm/php_vm.c", __FILE__)) {|f| f.read}
                        m = txt.match(/rb_define_const\(rb_mPHPVM, "VERSION", rb_str_new2\("([^"]+)"\)\)/)
                        version = m[1] if m
                        version
                      }.call
end
