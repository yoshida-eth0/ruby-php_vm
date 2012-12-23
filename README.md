php_vm
======================
php_vm is a native bridge between Ruby and PHP.

Install
------
	$ gem install php_vm

PHP(sapi/embed) required.


Install PHP (sapi/embed)
------
	$ git clone git://github.com/php/php-src.git
	$ cd php-src
	$ ./buildconf
	$ ./configure --prefix=/usr/local --enable-embed=shared --disable-cli --disable-cgi --without-pear
	$ make
	$ sudo make install
