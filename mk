#!/bin/sh -x

unset DEB_BUILD_HARDENING
pkgs="glib-2.0 gsl"

# security
#sflags="-D_FORIFY_SOURCE=2 -Wformat -Wformat-security -pie -fPIE -Wl,-z,relro -Wl,-z,now -fstack-protector"

# maximum compatibility
wflags="-pedantic -Wall -Wfloat-equal -Wbad-function-cast -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wdeclaration-after-statement -Wuninitialized -Werror -Wstrict-overflow=5 -Wextra -Wno-unused-parameter -Wfatal-errors"
#dflags="-g"
#dflags="-ggdb3 -pg --coverage"
dflags="-g -pg"
cflags="-std=c89 -O2 $sflags $wflags $dflags $(pkg-config --cflags $pkgs)"

lflags="$(pkg-config --libs $pkgs)"
cc $cflags -o fastbrt fastbrt.c $lflags
