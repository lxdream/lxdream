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
        gui.c gui.h gui_mmr.c \
        asic.c asic.h pvr2.c pvr2.h \
        video.c machine.c aica.c aica.h\
        fileio.c ipbin.c

dream_LDADD = @PACKAGE_LIBS@ $(INTLLIBS) sh4/libsh4.a 

AM_CFLAGS = -D_ISOC99_SOURCE -D_BSD_SOURCE 
