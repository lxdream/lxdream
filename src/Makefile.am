## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_CONF_DIR=\""$(sysconfdir)"\" \
        -Ish4 \
	@PACKAGE_CFLAGS@ @ESOUND_CFLAGS@

bin_PROGRAMS = lxdream
noinst_PROGRAMS = gendec genglsl

BUILT_SOURCES = sh4/sh4core.c sh4/sh4dasm.c sh4/sh4x86.c drivers/gl_slsrc.c

gendec_SOURCES = tools/gendec.c tools/gendec.h tools/insparse.c tools/actparse.c
genglsl_SOURCES = tools/genglsl.c

lxdream_SOURCES = \
        main.c config.c config.h \
        mem.c mem.h mmio.h watch.c \
        asic.c asic.h \
        syscall.c syscall.h bios.c dcload.c \
        gdrom/ide.c gdrom/ide.h gdrom/packet.h gdrom/gdimage.c \
	gdrom/gdrom.c gdrom/gdrom.h gdrom/nrg.c gdrom/cdi.c gdrom/gdi.c \
        dreamcast.c dreamcast.h eventq.c eventq.h \
        sh4/sh4.c sh4/intc.c sh4/intc.h sh4/sh4mem.c sh4/timer.c sh4/dmac.c \
        sh4/sh4core.c sh4/sh4core.h sh4/sh4dasm.c sh4/sh4dasm.h \
        sh4/sh4mmio.c sh4/sh4mmio.h sh4/scif.c sh4/sh4stat.c sh4/sh4stat.h \
	sh4/xltcache.c sh4/xltcache.h \
        aica/armcore.c aica/armcore.h aica/armdasm.c aica/armmem.c \
        aica/aica.c aica/aica.h aica/audio.c aica/audio.h \
	pvr2/pvr2.c pvr2/pvr2.h pvr2/pvr2mem.c \
	pvr2/tacore.c pvr2/render.c pvr2/rendcore.c pvr2/rendbkg.c pvr2/rendsort.c \
	pvr2/texcache.c pvr2/yuv.c pvr2/rendsave.c \
        maple/maple.c maple/maple.h \
        maple/controller.c maple/controller.h \
        gui/gtkui.c gui/gtkui.h gui/main_win.c gui/gtkcb.c \
        gui/mmio_win.c gui/debug_win.c gui/dump_win.c \
        gui/ctrl_dlg.c gui/path_dlg.c gui/gdrom_menu.c \
        loader.c bootstrap.c util.c \
	display.c display.h \
	drivers/audio_null.c \
	drivers/video_null.c drivers/video_gtk.c drivers/video_gtk.h \
	drivers/video_x11.c drivers/video_x11.h \
	drivers/gl_common.c drivers/gl_common.h drivers/gl_fbo.c \
	drivers/gl_sl.c drivers/gl_slsrc.c

if BUILD_SH4X86
lxdream_SOURCES += sh4/sh4x86.c \
        sh4/sh4trans.c sh4/sh4trans.h \
        x86dasm/x86dasm.c x86dasm/x86dasm.h \
        x86dasm/i386-dis.c x86dasm/dis-init.c x86dasm/dis-buf.c
endif

if CDROM_LINUX
lxdream_SOURCES += drivers/cd_linux.c
else
lxdream_SOURCES += drivers/cd_none.c
endif

if AUDIO_ESOUND
lxdream_SOURCES += drivers/audio_esd.c
endif

lxdream_LDADD = @PACKAGE_LIBS@ @ESOUND_LIBS@ $(INTLLIBS)

gendec_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)
genglsl_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

TESTS = test/testxlt

check_PROGRAMS = test/testxlt test/testsh4x86

test_testxlt_SOURCES = test/testxlt.c sh4/xltcache.c sh4/xltcache.h

test_testsh4x86_SOURCES = test/testsh4x86.c x86dasm/x86dasm.c \
	x86dasm/x86dasm.h x86dasm/i386-dis.c x86dasm/dis-init.c \
	x86dasm/dis-buf.c \
	sh4/sh4dasm.c sh4/sh4trans.c sh4/sh4x86.c sh4/xltcache.c \
	sh4/xltcache.h mem.c util.c
test_testsh4x86_LDADD = @PACKAGE_LIBS@

AM_CFLAGS = -D_ISOC99_SOURCE -D_BSD_SOURCE 

sh4/sh4core.c: gendec sh4/sh4.def sh4/sh4core.in
	./gendec sh4/sh4.def sh4/sh4core.in -o sh4/sh4core.c
sh4/sh4dasm.c: gendec sh4/sh4.def sh4/sh4dasm.in
	./gendec sh4/sh4.def sh4/sh4dasm.in -o sh4/sh4dasm.c
sh4/sh4x86.c: gendec sh4/sh4.def sh4/sh4x86.in
	./gendec sh4/sh4.def sh4/sh4x86.in -o sh4/sh4x86.c
sh4/sh4stat.c: gendec sh4/sh4.def sh4/sh4stat.in
	./gendec sh4/sh4.def sh4/sh4stat.in -o sh4/sh4stat.c
drivers/gl_slsrc.c: genglsl drivers/vertex.glsl drivers/fragment.glsl
	./genglsl drivers/vertex.glsl drivers/fragment.glsl drivers/gl_slsrc.c
