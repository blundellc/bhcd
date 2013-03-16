#!/bin/sh -x

pkgs="glib-2.0 gsl"

# maximum compatibility
wflags="-pedantic -Wall -Wfloat-equal -Wbad-function-cast -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wdeclaration-after-statement -Wuninitialized -Werror -Wstrict-overflow=5 -Wextra -Wno-unused-parameter -Wfatal-errors"
dflags="-g"
#dflags="-ggdb3 -pg --coverage"
cflags="-std=c89 -O2 $wflags $dflags $(pkg-config --cflags $pkgs)"

lflags="$(pkg-config --libs $pkgs)"
cc $cflags -o fastbrt $lflags fastbrt.c
