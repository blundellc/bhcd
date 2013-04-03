include $(top_srcdir)/config/Make-rules

bin_PROGRAMS = fastnrt loadgml benchnrt test test_bitset_hash savetoy

lib_LTLIBRARIES = libnrt.la


libnrt_la_SOURCES = dataset.c util.c counts.c params.c tree.c merge.c \
					build.c tokens.c dataset_gml.c tree_io.c sscache.c \
					bitset.c labelset.c dataset_gen.c islands.c lua_nrt.c
libnrt_la_LIBADD = $(DEPS_LIBS)

fastnrt_SOURCES = fastnrt.c
fastnrt_LDADD = libnrt.la $(DEPS_LIBS)

benchnrt_SOURCES = benchnrt.c
benchnrt_LDADD = libnrt.la $(DEPS_LIBS)

loadgml_SOURCES = loadgml.c
loadgml_LDADD = libnrt.la $(DEPS_LIBS)

savetoy_SOURCES = savetoy.c
savetoy_LDADD = libnrt.la $(DEPS_LIBS)

test_SOURCES = test.c
test_LDADD = libnrt.la $(DEPS_LIBS)

test_bitset_hash_SOURCES = test_bitset_hash.c
test_bitset_hash_LDADD = libnrt.la $(DEPS_LIBS)

