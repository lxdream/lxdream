## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_srcdir)/intl -I$(top_srcdir)/src

noinst_LIBRARIES = libsh4.a
libsh4_a_SOURCES = mem.c mem.h sh4core.c sh4core.h sh4dasm.c sh4dasm.h \
                   sh4mmio.c sh4mmio.h mmio.h intc.c intc.h

AM_CFLAGS = -D_ISOC99_SOURCE -D_BSD_SOURCE
