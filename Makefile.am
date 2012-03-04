## Process this file with automake to produce Makefile.in

POD2MAN=@POD2MAN@
BUNDLE=lxdream.app
DMGFILE=lxdream-$(VERSION)-$(host_cpu).dmg
DMGLABEL=Lxdream-$(VERSION)-$(host_cpu)

if BUILD_SYSTEST
  TEST_SUBDIR = test
endif
SUBDIRS = src po $(TEST_SUBDIR)

EXTRA_DIST = autogen.sh lxdream.pod lxdream.desktop bundlelibs.pl

dist_sysconf_DATA = lxdreamrc
CLEANFILES = lxdream.1
man_MANS = lxdream.1

lxdream.1: lxdream.pod
	$(POD2MAN) -c "Lxdream: A Dreamcast emulator" $< $@

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/pixmaps/$(PACKAGE); \
	  for pixmap in $(srcdir)/pixmaps/*.png $(srcdir)/pixmaps/*.gif; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(DESTDIR)$(datadir)/pixmaps/$(PACKAGE); \
	    fi \
	  done \
	fi
	$(mkinstalldirs) $(DESTDIR)$(datadir)/applications
	$(INSTALL_DATA) $(srcdir)/lxdream.desktop $(DESTDIR)$(datadir)/applications
	$(INSTALL_DATA) $(srcdir)/pixmaps/lxdream.png $(DESTDIR)$(datadir)/pixmaps
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)

uninstall-local:
	@$(NORMAL_UNINSTALL) 
	rm -rf $(DESTDIR)$(datadir)/pixmaps/$(PACKAGE)
	rm -f $(DESTDIR)$(datadir)/pixmaps/lxdream.png
	rm -f $(DESTDIR)$(datadir)/applications/lxdream.desktop

clean-local:
	rm -rf $(BUNDLE)
               
dist-hook:
	if test -d $(srcdir)/pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi
	if test -d $(srcdir)/debian; then \
	  mkdir $(distdir)/debian; \
	  for f in $(srcdir)/debian/*; do \
	    if test -f $$f; then \
	      cp -p $$f $(distdir)/debian; \
	    fi \
	  done \
	fi

if GUI_COCOA
# Build a Mac OS X bundle. This is meaningless anywhere else
all-local:
	@echo --- Building Mac Bundle ---
	rm -rf $(BUNDLE)
	mkdir -p $(BUNDLE)/Contents/MacOS
	mkdir -p $(BUNDLE)/Contents/Resources
	mkdir -p $(BUNDLE)/Contents/Plugins
	cp Info.plist $(BUNDLE)/Contents
	cp $(srcdir)/lxdreamrc $(BUNDLE)/Contents/Resources
	cp src/lxdream $(BUNDLE)/Contents/MacOS
	cp -R src/*.dylib $(BUNDLE)/Contents/Plugins
	cp -R $(srcdir)/pixmaps/* $(BUNDLE)/Contents/Resources
	$(srcdir)/bundlelibs.pl $(BUNDLE)/Contents/MacOS/lxdream $(BUNDLE)/Contents/Frameworks
	for cat in $(CATALOGS); do \
	   catname=`basename "$$cat"`; \
	   catname=`echo $$catname|sed -e 's/$(CATOBJEXT)$$//'`; \
	   mkdir -p "$(BUNDLE)/Contents/Resources/$$catname/LC_MESSAGES"; \
	   cp "$(top_srcdir)/po/$$cat" "$(BUNDLE)/Contents/Resources/$$catname/LC_MESSAGES/lxdream$(INSTOBJEXT)"; \
	done
	@echo --- Done ---
endif

# Build a Mac DMG disk image.
dmg: $(BUNDLE)
	@echo --- Building DMG image ---
	hdiutil create -fs HFS+ -size 5m -volname $(DMGLABEL) $(DMGFILE)
	hdiutil attach -mountpoint tmp.mount $(DMGFILE)
	cp -Rdp $(BUNDLE) tmp.mount/
	cp dmg.layout tmp.mount/.DS_Store
	ln -s /Applications tmp.mount/Applications
	hdiutil unmount tmp.mount
	zip -9 $(DMGFILE).zip $(DMGFILE)
	rm $(DMGFILE)
	@echo --- Done ---

bundle: all

if GUI_ANDROID
all-local:
	(TARGETDIR="`pwd`/android" && cd $(srcdir)/android && \
	    $(ANT) -buildfile build.xml -Dsdk.dir=$(ANDROID_SDK_HOME) \
	       -Dout.dir="$$TARGETDIR" -Dnative.libs.dir="$$TARGETDIR/libs" \
	       -Dnative.libs.absolute.dir="$$TARGETDIR/libs" \
	       -Dtarget=$(ANDROID_SDK_VERSION) clean )
	$(mkdir_p) android/libs/$(ANDROID_ABI)
	$(INSTALL) src/liblxdream.so android/libs/$(ANDROID_ABI)/liblxdream.so
	$(INSTALL) $(ANDROID_GDBSERVER) android/libs/$(ANDROID_ABI)/gdbserver
	$(STRIP) --strip-unneeded android/libs/$(ANDROID_ABI)/liblxdream.so
	(TARGETDIR="`pwd`/android" && cd $(srcdir)/android && \
	    $(ANT) -buildfile build.xml -Dsdk.dir=$(ANDROID_SDK_HOME) \
	       -Dout.dir="$$TARGETDIR" -Dnative.libs.dir="$$TARGETDIR/libs" \
	       -Dnative.libs.absolute.dir="$$TARGETDIR/libs" \
	       -Dtarget=$(ANDROID_SDK_VERSION) debug )
	cp android/Lxdream-debug.apk Lxdream-debug.apk

install-adb:
	$(ANDROID_SDK_HOME)/platform-tools/adb install -r Lxdream-debug.apk

endif
