## Process this file with automake to produce Makefile.in
SVNVERSION=@SVNVERSION@
INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DPACKAGE_PLUGIN_DIR=\""$(pkglibdir)"\" \
	-DPACKAGE_CONF_DIR=\""$(sysconfdir)"\" \
        -Ish4 \
	@GLIB_CFLAGS@ @GTK_CFLAGS@ @LIBPNG_CFLAGS@ @PULSE_CFLAGS@ @ESOUND_CFLAGS@ @ALSA_CFLAGS@ @SDL_CFLAGS@ @LIBISOFS_CFLAGS@

SOEXT=@SOEXT@
PLUGINCFLAGS = @PLUGINCFLAGS@ 
PLUGINLDFLAGS = @PLUGINLDFLAGS@
bin_PROGRAMS = lxdream
noinst_PROGRAMS = gendec genglsl genmach
check_PROGRAMS = test/testxlt

pkglib_PROGRAMS=
EXTRA_DIST=drivers/genkeymap.pl checkver.pl drivers/dummy.c
AM_CFLAGS = -D__EXTENSIONS__ -D_BSD_SOURCE -D_GNU_SOURCE

.PHONY: checkversion
checkversion:
	@$(srcdir)/checkver.pl "$(VERSION)" "$(top_srcdir)" version.c

version.c: checkversion

TESTS = test/testxlt
BUILT_SOURCES = sh4/sh4core.c sh4/sh4dasm.c sh4/sh4x86.c sh4/sh4stat.c \
	pvr2/shaders.def pvr2/shaders.h drivers/mac_keymap.h version.c
CLEANFILES = sh4/sh4core.c sh4/sh4dasm.c sh4/sh4x86.c sh4/sh4stat.c \
	pvr2/shaders.def pvr2/shaders.h drivers/mac_keymap.h version.c  \
	audio_alsa.lo audio_sdl.lo audio_esd.lo audio_pulse.lo input_lirc.lo \
	lxdream_dummy.lo

#all-am: checkversion

gendec_SOURCES = tools/gendec.c tools/gendec.h tools/insparse.c tools/actparse.c
genmach_SOURCES = tools/genmach.c tools/genmach.h tools/mdparse.c
genglsl_SOURCES = tools/genglsl.c
lxdream_LINK = $(LINK) @LXDREAMLDFLAGS@
lxdream_LDADD = @LXDREAM_LIBS@ @GLIB_LIBS@ @GTK_LIBS@ @LIBPNG_LIBS@ @LIBISOFS_LIBS@ $(INTLLIBS)
lxdream_SOURCES = \
        main.c version.c config.c config.h lxdream.h dream.h gui.h cpu.c cpu.h hook.h \
        gettext.h mem.c mem.h sdram.c mmio.h watch.c \
        asic.c asic.h clock.h serial.h \
        syscall.c syscall.h bios.c dcload.c gdbserver.c \
        ioutil.c ioutil.h lxpaths.c lxpaths.h \
        gdrom/ide.c gdrom/ide.h gdrom/packet.h gdrom/gdrom.c gdrom/gdrom.h \
        dreamcast.c dreamcast.h eventq.c eventq.h \
        sh4/sh4.c sh4/intc.c sh4/intc.h sh4/sh4mem.c sh4/timer.c sh4/dmac.c \
        sh4/mmu.c sh4/sh4core.c sh4/sh4core.h sh4/sh4dasm.c sh4/sh4dasm.h \
        sh4/sh4mmio.c sh4/sh4mmio.h sh4/scif.c sh4/sh4stat.c sh4/sh4stat.h \
	xlat/xltcache.c xlat/xltcache.h sh4/sh4.h sh4/dmac.h sh4/pmm.c \
	sh4/cache.c sh4/mmu.h sh4/shadow.c \
        aica/armcore.c aica/armcore.h aica/armdasm.c aica/armdasm.h aica/armmem.c \
        aica/aica.c aica/aica.h aica/audio.c aica/audio.h \
	pvr2/pvr2.c pvr2/pvr2.h pvr2/pvr2mem.c pvr2/pvr2mmio.h pvr2/untile.c \
	pvr2/tacore.c pvr2/rendsort.c pvr2/tileiter.h pvr2/shaders.glsl \
	pvr2/texcache.c pvr2/yuv.c pvr2/rendsave.c pvr2/scene.c pvr2/scene.h \
	pvr2/gl_sl.c pvr2/shaders.h pvr2/shaders.def pvr2/glutil.c pvr2/glutil.h pvr2/glrender.c \
        maple/maple.c maple/maple.h \
        maple/controller.c maple/kbd.c maple/mouse.c maple/lightgun.c maple/vmu.c \
        loader.c loader.h elf.h bootstrap.c bootstrap.h util.c gdlist.c gdlist.h \
        vmu/vmuvol.c vmu/vmuvol.h vmu/vmulist.c vmu/vmulist.h \
	display.c display.h dckeysyms.h \
	drivers/audio_null.c drivers/video_null.c \
	drivers/video_gl.c drivers/video_gl.h drivers/gl_fbo.c \
	drivers/serial_unix.c \
	drivers/cdrom/cdrom.h drivers/cdrom/cdrom.c drivers/cdrom/drive.h \
	drivers/cdrom/sector.h drivers/cdrom/sector.c drivers/cdrom/defs.h \
        drivers/cdrom/cd_nrg.c drivers/cdrom/cd_cdi.c drivers/cdrom/cd_gdi.c \
        drivers/cdrom/edc_ecc.c drivers/cdrom/ecc.h drivers/cdrom/drive.c \
        drivers/cdrom/edc_crctable.h drivers/cdrom/edc_encoder.h \
	drivers/cdrom/edc_l2sq.h drivers/cdrom/edc_scramble.h drivers/cdrom/cd_mmc.c \
	drivers/cdrom/isofs.h drivers/cdrom/isofs.c \
	sh4/sh4.def sh4/sh4core.in sh4/sh4x86.in sh4/sh4dasm.in sh4/sh4stat.in \
	hotkeys.c hotkeys.h
lxdream_CPPFLAGS = @LXDREAMCPPFLAGS@
if BUILD_SHARED
lxdream_SOURCES += plugin.c plugin.h
endif	

if BUILD_SH4X86
lxdream_SOURCES += sh4/sh4x86.c xlat/x86/x86op.h \
        xlat/x86/ia32abi.h xlat/x86/amd64abi.h \
        sh4/sh4trans.c sh4/sh4trans.h sh4/mmux86.c \
        x86dasm/x86dasm.c x86dasm/x86dasm.h \
        x86dasm/i386-dis.c x86dasm/dis-init.c x86dasm/dis-buf.c \
        x86dasm/ansidecl.h x86dasm/bfd.h x86dasm/dis-asm.h \
        x86dasm/symcat.h x86dasm/sysdep.h

test_testsh4x86_LDADD = @LXDREAM_LIBS@ @GLIB_LIBS@ @GTK_LIBS@ @LIBPNG_LIBS@
test_testsh4x86_CPPFLAGS = @LXDREAMCPPFLAGS@
test_testsh4x86_SOURCES = test/testsh4x86.c x86dasm/x86dasm.c \
	x86dasm/x86dasm.h x86dasm/i386-dis.c x86dasm/dis-init.c \
	x86dasm/dis-buf.c \
	sh4/sh4trans.c sh4/sh4x86.c xlat/xltcache.c sh4/sh4dasm.c \
	xlat/xltcache.h mem.c util.c cpu.c

check_PROGRAMS += test/testsh4x86
endif

if GUI_GTK
lxdream_SOURCES += gtkui/gtkui.c gtkui/gtkui.h \
	gtkui/gtk_win.c gtkui/gtkcb.c gtkui/gtk_cfg.c \
        gtkui/gtk_mmio.c gtkui/gtk_debug.c gtkui/gtk_dump.c \
        gtkui/gtk_ctrl.c gtkui/gtk_gd.c \
        drivers/io_glib.c drivers/video_gtk.c
endif

if GUI_COCOA
lxdream_SOURCES += cocoaui/cocoaui.m cocoaui/cocoaui.h cocoaui/cocoa_cfg.m \
	cocoaui/cocoa_win.m cocoaui/cocoa_gd.m cocoaui/cocoa_prefs.m \
	cocoaui/cocoa_ctrl.m cocoaui/paths_osx.m \
	drivers/io_osx.m drivers/video_osx.m \
	drivers/mac_keymap.h drivers/mac_keymap.txt
else
lxdream_SOURCES += paths_unix.c
endif

if VIDEO_OSMESA
lxdream_SOURCES += drivers/video_gdk.c
endif

if VIDEO_GLX 
lxdream_SOURCES += drivers/video_glx.c drivers/video_glx.h
endif

if VIDEO_NSGL
lxdream_SOURCES += drivers/video_nsgl.m drivers/video_nsgl.h
endif

if AUDIO_OSX
lxdream_SOURCES += drivers/audio_osx.m
endif

if BUILD_SHARED

pkglib_PROGRAMS += lxdream_dummy.@SOEXT@
lxdream_dummy_@SOEXT@_SOURCES =
lxdream_dummy_@SOEXT@_LDADD = lxdream_dummy.lo @SDL_LIBS@
lxdream_dummy_@SOEXT@_LDFLAGS = $(PLUGINLDFLAGS)
lxdream_dummy.lo: drivers/dummy.c
	$(COMPILE) -DPLUGIN $(PLUGINCFLAGS) -c $< -o $@

if AUDIO_SDL
pkglib_PROGRAMS += audio_sdl.@SOEXT@
audio_sdl_@SOEXT@_SOURCES =
audio_sdl_@SOEXT@_LDADD = audio_sdl.lo @SDL_LIBS@
audio_sdl_@SOEXT@_LDFLAGS = $(PLUGINLDFLAGS)
audio_sdl.lo: drivers/audio_sdl.c
	$(COMPILE) -DPLUGIN $(PLUGINCFLAGS) -c $< -o $@
endif

if AUDIO_PULSE
pkglib_PROGRAMS += audio_pulse.@SOEXT@
audio_pulse_@SOEXT@_SOURCES =
audio_pulse_@SOEXT@_LDADD = audio_pulse.lo @PULSE_LIBS@
audio_pulse_@SOEXT@_LDFLAGS = $(PLUGINLDFLAGS)
audio_pulse.lo: drivers/audio_pulse.c
	$(COMPILE) -DPLUGIN $(PLUGINCFLAGS) -c $< -o $@
endif

if AUDIO_ESOUND
pkglib_PROGRAMS += audio_esd.@SOEXT@
audio_esd_@SOEXT@_SOURCES =
audio_esd_@SOEXT@_LDADD = audio_esd.lo @ESOUND_LIBS@
audio_esd_@SOEXT@_LDFLAGS = $(PLUGINLDFLAGS)
audio_esd.lo: drivers/audio_esd.c
	$(COMPILE) -DPLUGIN $(PLUGINCFLAGS) -c $< -o $@
endif

if AUDIO_ALSA
pkglib_PROGRAMS += audio_alsa.@SOEXT@
audio_alsa_@SOEXT@_SOURCES =
audio_alsa_@SOEXT@_LDADD = audio_alsa.lo @ALSA_LIBS@
audio_alsa_@SOEXT@_LDFLAGS = $(PLUGINLDFLAGS)
audio_alsa.lo: drivers/audio_alsa.c
	$(COMPILE) -DPLUGIN $(PLUGINCFLAGS) -c $< -o $@
endif

if INPUT_LIRC
pkglib_PROGRAMS += input_lirc.@SOEXT@
input_lirc_@SOEXT@_SOURCES =
input_lirc_@SOEXT@_LDADD = input_lirc.lo	 -llirc_client
input_lirc_@SOEXT@_LDFLAGS = $(PLUGINLDFLAGS)
input_lirc.lo: drivers/input_lirc.c
	$(COMPILE) -DPLUGIN $(PLUGINCFLAGS) -c $< -o $@
endif

else # build statically
if AUDIO_SDL
lxdream_SOURCES += drivers/audio_sdl.c
lxdream_LDADD += @SDL_LIBS@
endif

if AUDIO_PULSE
lxdream_SOURCES += drivers/audio_pulse.c
lxdream_LDADD += @PULSE_LIBS@
endif

if AUDIO_ESOUND
lxdream_SOURCES += drivers/audio_esd.c
lxdream_LDADD += @ESOUND_LIBS@
endif

if AUDIO_ALSA
lxdream_SOURCES += drivers/audio_alsa.c
lxdream_LDADD += @ALSA_LIBS@
endif

if INPUT_LIRC
lxdream_SOURCES += drivers/input_lirc.c
lxdream_LDADD += -llirc_client
endif
endif # end shared/static control

if CDROM_LINUX
lxdream_SOURCES += drivers/cdrom/cd_linux.c
endif

if CDROM_OSX
lxdream_SOURCES += drivers/cdrom/cd_osx.c drivers/osx_iokit.m drivers/osx_iokit.h
endif

if CDROM_NONE
lxdream_SOURCES += drivers/cdrom/cd_none.c
endif

if JOY_LINUX
lxdream_SOURCES += drivers/joy_linux.c drivers/joy_linux.h
endif



gendec_LDADD = @GLIB_LIBS@ @GTK_LIBS@ $(INTLLIBS)
genglsl_LDADD = @GLIB_LIBS@ @GTK_LIBS@ $(INTLLIBS)
genmach_LDADD = @GLIB_LIBS@ @GTK_LIBS@ $(INTLLIBS)


test_testxlt_SOURCES = test/testxlt.c xlat/xltcache.c xlat/xltcache.h

sh4/sh4core.c: gendec sh4/sh4.def sh4/sh4core.in
	$(mkdir_p) `dirname $@`
	./gendec $(srcdir)/sh4/sh4.def $(srcdir)/sh4/sh4core.in -o $@
sh4/sh4dasm.c: gendec sh4/sh4.def sh4/sh4dasm.in
	$(mkdir_p) `dirname $@`
	./gendec $(srcdir)/sh4/sh4.def $(srcdir)/sh4/sh4dasm.in -o $@
sh4/sh4x86.c: gendec sh4/sh4.def sh4/sh4x86.in
	$(mkdir_p) `dirname $@`
	./gendec $(srcdir)/sh4/sh4.def $(srcdir)/sh4/sh4x86.in -o $@
sh4/sh4stat.c: gendec sh4/sh4.def sh4/sh4stat.in
	$(mkdir_p) `dirname $@`
	./gendec $(srcdir)/sh4/sh4.def $(srcdir)/sh4/sh4stat.in -o $@
pvr2/shaders.def: genglsl pvr2/shaders.glsl
	$(mkdir_p) `dirname $@`
	./genglsl $(srcdir)/pvr2/shaders.glsl $@
pvr2/shaders.h: pvr2/shaders.def
drivers/mac_keymap.h: drivers/mac_keymap.txt drivers/genkeymap.pl
	$(mkdir_p) `dirname $@`
	$(srcdir)/drivers/genkeymap.pl mac $(srcdir)/drivers/mac_keymap.txt > $@
	