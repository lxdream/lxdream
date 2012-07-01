
AM_CPPFLAGS = -I$(top_srcdir)/src $(GLIB_FOR_BUILD_CFLAGS)
AM_LDFLAGS = $(GLIB_FOR_BUILD_LDFLAGS)
CC = $(CC_FOR_BUILD)
CPPFLAGS = $(CPPFLAGS_FOR_BUILD)
CFLAGS = $(CFLAGS_FOR_BUILD)
LDFLAGS = $(LDFLAGS_FOR_BUILD)

noinst_PROGRAMS = gendec genglsl genmach

gendec_SOURCES = gendec.c gendec.h insparse.c actparse.c
gendec_LDADD = @GLIB_FOR_BUILD_LIBS@ @GTK_LIBS@ $(INTLLIBS)

genmach_SOURCES = genmach.c genmach.h mdparse.c
genmach_LDADD = @GLIB_FOR_BUILD_LIBS@ @GTK_LIBS@ $(INTLLIBS)

genglsl_SOURCES = genglsl.c
genglsl_LDADD = @GLIB_FOR_BUILD_LIBS@ @GTK_LIBS@ $(INTLLIBS)

