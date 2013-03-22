include $(top_srcdir)/config/Make-rules

bin_PROGRAMS = fastnrt loadgml benchnrt test

core_srcs = dataset.c util.c counts.c params.c tree.c merge.c \
					build.c tokens.c dataset_gml.c tree_io.c sscache.c \
					bitset.c labelset.c dataset_gen.c

fastnrt_SOURCES = fastnrt.c $(core_srcs)
fastnrt_LDADD = $(DEPS_LIBS)

benchnrt_SOURCES = benchnrt.c $(core_srcs)
benchnrt_LDADD = $(DEPS_LIBS)

loadgml_SOURCES = loadgml.c $(core_srcs)
loadgml_LDADD = $(DEPS_LIBS)

test_SOURCES = test.c $(core_srcs)
test_LDADD = $(DEPS_LIBS)

