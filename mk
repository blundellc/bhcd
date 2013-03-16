#!/bin/sh -x

# clang -std=c99 -Wall -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wmissing-prototypes -g -o fastbrt $(pkg-config --cflags --libs glib-2.0 gsl) fastbrt.c

pkgs="glib-2.0 gsl"

# maximum compatibility
wflags="-pedantic -Wall -Wfloat-equal -Wbad-function-cast -Wshadow -Wpointer-arith -Wcast-qual -Wstrict-prototypes -Wdeclaration-after-statement"
dflags="-g"
#dflags="-ggdb3 -pg --coverage"
cflags="-std=c89 -O2 $wflags $dflags $(pkg-config --cflags $pkgs)"

lflags="$(pkg-config --libs $pkgs)"
cc $cflags -o fastbrt $lflags fastbrt.c
