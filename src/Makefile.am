include $(top_srcdir)/config/Make-rules

bin_PROGRAMS = fastnrt loadgml

fastnrt_SOURCES = fastnrt.c dataset.c util.c counts.c params.c tree.c merge.c build.c tokens.c dataset_gml.c
fastnrt_LDADD = $(DEPS_LIBS)

loadgml_SOURCES = dataset.c dataset_gml.c tokens.c loadgml.c
loadgml_LDADD = $(DEPS_LIBS)

