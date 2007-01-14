## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
        -Ish4 \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = lxdream

lxdream_SOURCES = \
        main.c \
        mem.c mem.h mmio.h watch.c \
        asic.c asic.h \
        syscall.c syscall.h bios.c dcload.c \
        gdrom/ide.c gdrom/ide.h gdrom/packet.h \
	gdrom/gdrom.c gdrom/gdrom.h gdrom/nrg.c gdrom/cdi.c gdrom/linux.c \
        dreamcast.c dreamcast.h eventq.c eventq.h \
        sh4/intc.c sh4/intc.h sh4/sh4mem.c sh4/timer.c sh4/dmac.c \
        sh4/sh4core.c sh4/sh4core.h sh4/sh4dasm.c sh4/sh4dasm.h \
        sh4/sh4mmio.c sh4/sh4mmio.h sh4/scif.c \
        aica/armcore.c aica/armcore.h aica/armdasm.c aica/armmem.c \
        aica/aica.c aica/aica.h aica/audio.c aica/audio.h \
	pvr2/pvr2.c pvr2/pvr2.h \
	pvr2/tacore.c pvr2/render.c pvr2/rendcore.c pvr2/rendbkg.c pvr2/rendsort.c \
	pvr2/texcache.c \
        maple/maple.c maple/maple.h \
        maple/controller.c maple/controller.h \
        gui/support.c gui/support.h \
        gui/interface.c gui/interface.h \
        gui/callbacks.c gui/callbacks.h \
        gui/gui.c gui/gui.h gui/mmr_win.c \
        gui/debug_win.c gui/dump_win.c \
        loader.c bootstrap.c util.c \
	display.c display.h \
	drivers/audio_null.c drivers/audio_esd.c \
	drivers/video_null.c drivers/video_gtk.c drivers/video_gtk.h \
	drivers/video_x11.c drivers/video_x11.h \
	drivers/video_gl.c drivers/video_gl.h

lxdream_LDADD = @PACKAGE_LIBS@ $(INTLLIBS) -lesd

AM_CFLAGS = -D_ISOC99_SOURCE -D_BSD_SOURCE 
