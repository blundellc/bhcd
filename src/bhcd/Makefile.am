include $(top_srcdir)/config/Make-rules

bin_PROGRAMS = bhcd loadgml test test_bitset_hash savetoy pagerank

lib_LTLIBRARIES = libbhcd.la

DEPS_CFLAGS += -I$(top_srcdir)/src/hccd
DEPS_LIBS += ../hccd/libhccd.la

libbhcd_la_SOURCES = dataset.c params.c tree.c merge.c build.c \
					 dataset_gml.c tree_io.c sscache.c labelset.c \
					 dataset_gen.c islands.c lua_bhcd.c
libbhcd_la_LIBADD = $(DEPS_LIBS)
libbhcd_la_CPPFLAGS = -I$(top_srcdir)/src/hccd

bhcd_SOURCES = bhcd.c
bhcd_LDADD = libbhcd.la $(DEPS_LIBS)

#benchbhcd_SOURCES = benchbhcd.c
#benchbhcd_LDADD = libbhcd.la $(DEPS_LIBS)

loadgml_SOURCES = loadgml.c
loadgml_LDADD = libbhcd.la $(DEPS_LIBS)

savetoy_SOURCES = savetoy.c
savetoy_LDADD = libbhcd.la $(DEPS_LIBS)

test_SOURCES = test.c
test_LDADD = libbhcd.la $(DEPS_LIBS)

test_bitset_hash_SOURCES = test_bitset_hash.c
test_bitset_hash_LDADD = libbhcd.la $(DEPS_LIBS)

pagerank_SOURCES = pagerank.c
pagerank_LDADD = libbhcd.la $(DEPS_LIBS)

BUILT_SOURCES += version.stamp
CLEANFILES += git_version.h git_version.h.tmp

version.stamp:
	echo '#define	GIT_VERSION	"git-'`git describe --always`'"'>git_version.h.tmp
	cmp git_version.h.tmp git_version.h || mv git_version.h.tmp git_version.h

