## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
        -Ish4 \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = dream

dream_SOURCES = \
        main.c \
        mem.c mem.h mmio.h \
        asic.c asic.h \
        pvr2.c pvr2.h \
        gdrom/ide.c gdrom/ide.h \
        video.c dreamcast.c dreamcast.h \
        sh4/intc.c sh4/intc.h sh4/sh4mem.c sh4/timer.c \
        sh4/sh4core.c sh4/sh4core.h sh4/sh4dasm.c sh4/sh4dasm.h \
        sh4/sh4mmio.c sh4/sh4mmio.h sh4/scif.c sh4/watch.c \
        aica/armcore.c aica/armcore.h aica/armdasm.c aica/armmem.c \
        aica/aica.c aica/aica.h \
        maple/maple.c maple/maple.h \
        maple/controller.c maple/controller.h \
        gui/support.c gui/support.h \
        gui/interface.c gui/interface.h \
        gui/callbacks.c gui/callbacks.h \
        gui/gui.c gui/gui.h gui/mmr_win.c \
        gui/debug_win.c gui/dump_win.c \
        loader.c bootstrap.c util.c

dream_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

AM_CFLAGS = -D_ISOC99_SOURCE -D_BSD_SOURCE 
