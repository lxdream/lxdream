#!/bin/sh

TAR=tar
PATCH=patch
MAKE=make
WGET=wget

GETTEXT=gettext-0.18.1.1
LIBICONV=libiconv-1.13.1
LIBPNG=libpng-1.4.3
LIBISOFS=libisofs-0.6.38
GLIB=glib-2.26.0

TARGETPREFIX="${HOME}/android/usr"
NDK_BINDIR="${HOME}/lxdream/android/arm-linux-androideabi-4.4.3/bin"
PATH="${NDK_BINDIR}:$PATH"
export PATH
BUILDALIAS=`gcc -dumpmachine`

if [ ! -e $TARGETPREFIX/lib/libiconv.a ]; then
  ${WGET} http://ftp.gnu.org/gnu/libiconv/${LIBCONV}.tar.gz
  ${TAR} -xzf ${LIBCONV}.tar.gz
  cp config.guess config.sub ${LIBCONV}/build-aux
  cp config.guess config.sub ${LIBCONV}/libcharset/build-aux
  ${PATCH} -p0 < libiconv-1.13.1.diff
  mkdir -p build-${LIBCONV}
  cd build-${LIBCONV}
  ../${LIBCONV}/configure  --prefix=$TARGETPREFIX --build=$BUILDALIAS --host=arm-linux-androideabi --disable-shared 'CPPFLAGS=-fPIC'
  ${MAKE} all install
  cd ..
fi

if [ ! -e $TARGETPREFIX/lib/libgettextpo.a ]; then
  ${WGET} http://ftp.gnu.org/gnu/getttext/${GETTEXT}.tar.gz
  ${TAR} -xzf ${GETTEXT}.tar.gz
  ${PATCH} -p0 < ${GETTEXT}.diff
  mkdir -p build-${GETTEXT}
  cd build-${GETTEXT}
  ../${GETTEXT}/configure  --prefix=$TARGETPREFIX --build=$BUILDALIAS --host=arm-linux-androideabi --disable-shared "CPPFLAGS=-I$TARGETPREFIX/include -fPIC" "LDFLAGS=-L$TARGETPREFIX/lib"
  ${MAKE} all install
  cd ..
fi

if [ ! -e $TARGETPREFIX/lib/libpng14.a ]; then
  ${WGET} http://sourceforge.net/projects/libpng/files/libpng14/older-releases/1.4.3/${LIBPNG}.tar.gz/download
  ${TAR} -xzf ${LIBPNG}.tar.gz
  cp config.guess config.sub ${LIBPNG}
  mkdir -p build-${LIBPNG}
  cd build-${LIBPNG}
  ../${LIBPNG}/configure  --prefix=$TARGETPREFIX --build=$BUILDALIAS --host=arm-linux-androideabi --disable-shared "CPPFLAGS=-I$TARGETPREFIX/include -fPIC" "LDFLAGS=-L$TARGETPREFIX/lib"
  ${MAKE} all install
  cd ..
fi

if [ ! -e $TARGETPREFIX/lib/libisofs.a ]; then
  ${WGET} http://files.libburnia-project.org/releases/${LIBISOFS}.tar.gz
  ${TAR} -xzf ${LIBISOFS}.tar.gz
  cp config.guess config.sub ${LIBISOFS}
  ${PATCH} -p0 < ${LIBISOFS}.diff
  cd ${LIBISOFS}
  ./configure  --prefix=$TARGETPREFIX --build=$BUILDALIAS --host=arm-linux-androideabi --disable-shared "CPPFLAGS=-I$TARGETPREFIX/include -fPIC" "LDFLAGS=-L$TARGETPREFIX/lib" LIBISOFS_ASSUME_ICONV=yes
  ${MAKE} all install THREAD_LIBS=
  cd ..
fi

if [ ! -e $TARGETPREFIX/lib/libglib-2.0.a ]; then
  ${WGET} http://ftp.gnome.org/pub/gnome/sources/glib/2.26/${GLIB}.tar.bz2
  ${TAR} -xjf ${GLIB}.tar.bz2
  cp config.guess config.sub ${GLIB}
  ${PATCH} -p0 < ${GLIB}.diff
  mkdir -p build-${GLIB}
  cp ${GLIB}.cache build-${GLIB}
  cd build-${GLIB}
  ../${GLIB}/configure  --prefix=$TARGETPREFIX --build=$BUILDALIAS --host=arm-linux-androideabi --disable-shared "CPPFLAGS=-I$TARGETPREFIX/include -fPIC" "LDFLAGS=-L$TARGETPREFIX/lib" \
      --cache-file=${GLIB}.cache --without-threads
  ${MAKE} install
  cd ..
fi

