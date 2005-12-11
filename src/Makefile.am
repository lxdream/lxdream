## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
        -Ish4 \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = dream

dream_SOURCES = \
        main.c \
        support.c support.h \
        interface.c interface.h \
        callbacks.c callbacks.h \
        gui.c gui.h mmr_win.c debug_win.c dump_win.c \
        mem.c mem.h mmio.h \
        asic.c asic.h pvr2.c pvr2.h ide.c ide.h \
        video.c dreamcast.c dreamcast.h \
        maple.c maple.h maple/controller.c maple/controller.h \
        sh4/intc.c sh4/intc.h sh4/sh4mem.c \
        sh4/sh4core.c sh4/sh4core.h sh4/sh4dasm.c sh4/sh4dasm.h \
        sh4/sh4mmio.c sh4/sh4mmio.h sh4/watch.c \
        aica/armcore.c aica/armcore.h aica/armdasm.c aica/armmem.c \
        aica/aica.c aica/aica.h \
        fileio.c ipbin.c

dream_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

AM_CFLAGS = -D_ISOC99_SOURCE -D_BSD_SOURCE 
