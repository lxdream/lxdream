## Process this file with automake to produce Makefile.in

if BUILD_SYSTEST
  TEST_SUBDIR = test
endif
SUBDIRS = src po $(TEST_SUBDIR)

EXTRA_DIST = autogen.sh

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
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi

# Build a Mac OS X bundle. This is meaningless anywhere else
bundle: all
	mkdir -p lxdream.app/Contents/MacOS
	mkdir -p lxdream.app/Contents/Resources
	cp src/lxdream lxdream.app/Contents/MacOS
	cp -R pixmaps/* lxdream.app/Contents/Resources
