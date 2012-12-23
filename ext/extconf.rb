require "mkmf"

if !have_library('php5', 'php_embed_init')
  exit(1)
end

$CFLAGS += " "+`php-config --includes`
$LOCAL_LIBS += ""
create_makefile("php_vm")
