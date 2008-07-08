## Process this file with automake to produce Makefile.in

POD2MAN=@POD2MAN@
BUNDLE=lxdream.app

if BUILD_SYSTEST
  TEST_SUBDIR = test
endif
SUBDIRS = src po $(TEST_SUBDIR)

EXTRA_DIST = autogen.sh lxdream.pod bundlelibs.pl

man_MANS = lxdream.1

lxdream.1: lxdream.pod
	$(POD2MAN) $< $@

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/pixmaps/$(PACKAGE); \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(DESTDIR)$(datadir)/pixmaps/$(PACKAGE); \
	    fi \
	  done \
	fi
	$(mkinstalldirs) $(DESTDIR)$(sysconfdir)
	$(INSTALL_DATA) lxdreamrc $(DESTDIR)$(sysconfdir)

dist-hook:
	if test -d $(srcdir)/pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
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
	cp src/lxdream $(BUNDLE)/Contents/MacOS
	cp -R $(srcdir)/pixmaps/* $(BUNDLE)/Contents/Resources
	$(srcdir)/bundlelibs.pl $(BUNDLE)/Contents/MacOS/lxdream $(BUNDLE)/Contents/Frameworks
	for cat in $(CATALOGS); do \
	   catname=`basename "$$cat"`; \
	   catname=`echo $$catname|sed -e 's/$(CATOBJEXT)$$//'`; \
	   mkdir -p "$(BUNDLE)/Contents/Resources/$$catname/LC_MESSAGES"; \
	   cp "po/$$cat" "$(BUNDLE)/Contents/Resources/$$catname/LC_MESSAGES/lxdream$(CATOBJEXT)"; \
	done
	@echo --- Done ---
endif

bundle: all
