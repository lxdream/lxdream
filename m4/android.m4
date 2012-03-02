# LX_ANDROID_BUILD
# Defines --with-android, --with-android-ndk, and --with-android-version
# If specified, checks for a working install, and sets build parameters
# appropriately for an Android host. 
AC_DEFUN([LX_ANDROID_BUILD], [
   AC_REQUIRE([AC_CANONICAL_HOST])
   AC_ARG_WITH( android, AS_HELP_STRING( [--with-android=SDK], [Specify the location of the Android SDK] ) )
   AC_ARG_WITH( android-ndk, AS_HELP_STRING( [--with-android-ndk=NDK], [Specify the location of the Android NDK] ) )
   AC_ARG_WITH( android-version, AS_HELP_STRING( [--with-android-version], [Specify target Android SDK version]), [], [with_android_version="android-11"] )
   AC_ARG_WITH( android-ndk-version, AS_HELP_STRING( [--with-android-version], [Specify target Android NDK version]), [], [with_ndk_version="android-9"] )

   if test "x$with_android" != "x"; then
      if test "$with_android" = "yes"; then
         AC_MSG_ERROR( [--with-android option must be given with the path to the Android SDK] )
      fi
      if test "x$with_android_ndk" = "x" -o "x$with_android_ndk" = "xyes"; then
         AC_MSG_ERROR( [--with-android-ndk=/path/to/ndk option must be used with --with-android] )
      fi

      ANDROID_SDK_HOME="$with_android"
      ANDROID_NDK_HOME="$with_android_ndk"
      ANDROID_SDK_VERSION="$with_android_version"
      ANDROID_NDK_VERSION="$with_ndk_version"
      
      AC_CHECK_FILE( [$ANDROID_SDK_HOME/tools/ant/pre_setup.xml], [], [ AC_MSG_ERROR([Android SDK not found in $ANDROID_SDK_HOME]) ])
      AC_CHECK_FILE( [$ANDROID_SDK_HOME/platforms/$ANDROID_SDK_VERSION/sdk.properties], [], [ AC_MSG_ERROR([Android platform version $ANDROID_SDK_VERSION not found in $ANDROID_SDK_HOME]) ])
      AC_CHECK_FILE( [$ANDROID_NDK_HOME/toolchains], [], [ AC_MSG_ERROR([Android NDK not found in $ANDROID_NDK_HOME]) ])

      case $host_alias in
         arm-* | "") 
            host_alias="arm-linux-androideabi"
            host_cpu="arm"
            host_vendor="unknown";
            host_os="linux-androideabi"
            ANDROID_NDK_BIN=`echo $ANDROID_NDK_HOME/toolchains/arm-*/prebuilt/*/bin`
            ANDROID_GDBSERVER=`echo $ANDROID_NDK_HOME/toolchains/arm-*/prebuilt/gdbserver`
            ANDROID_SYSROOT="$ANDROID_NDK_HOME/platforms/$ANDROID_NDK_VERSION/arch-arm"
            TARGETFLAGS="-ffunction-sections -funwind-tables -fstack-protector -fomit-frame-pointer -fno-strict-aliasing -finline-limit=64 -DANDROID -Wno-psabi -Wa,--noexecstack"
            TARGETFLAGS="$TARGETFLAGS -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__ -march=armv5te -mtune=xscale -msoft-float -mthumb -Os"
            ;;
         i686-*)
            host_alias="i686-android-linux"
            host_cpu="i686"
            host_vendor="android"
            host_os="linux"
            ANDROID_NDK_BIN=`echo $ANDROID_NDK_HOME/toolchains/x86-*/prebuilt/*/bin`
            ANDROID_GDBSERVER=`echo $ANDROID_NDK_HOME/toolchains/x86-*/prebuilt/gdbserver`
            ANDROID_SYSROOT="$ANDROID_NDK_HOME/platforms/$ANDROID_NDK_VERSION/arch-x86"
            TARGETFLAGS=""
            ;;
         *)
            AC_MSG_ERROR([Unsupported android host $host_alias])
      	    ;;
      esac
   
      AC_PATH_PROG( ANT, [ant] )
      
      CC="$ANDROID_NDK_BIN/${host_alias}-gcc"
      CXX="$ANDROID_NDK_BIN/${host_alias}-g++"
      CPP="$ANDROID_NDK_BIN/${host_alias}-cpp"
      LD="$ANDROID_NDK_BIN/${host_alias}-ld"
      AR="$ANDROID_NDK_BIN/${host_alias}-ar"
      RANLIB="$ANDROID_NDK_BIN/${host_alias}-ranlib"
      STRIP="$ANDROID_NDK_BIN/${host_alias}-strip"
      OBJDUMP="$ANDROID_NDK_BIN/${host_alias}-objdump"
      CPPFLAGS="-fPIC --sysroot=$ANDROID_SYSROOT -I$ANDROID_SYSROOT/usr/include $TARGETFLAGS $CPPFLAGS"
      LDFLAGS="-nostdlib -Wl,--no-undefined -L${ANDROID_SYSROOT}/usr/lib -Wl,-rpath-link,${ANDROID_SYSROOT}/usr/lib -Wl,-allow-shlib-undefined -Wl,-z,noexecstack $LDFLAGS"
      LIBS="$LIBS -liconv -landroid -llog -lgcc -lc -lm"
      
      AC_SUBST(ANDROID_SDK_HOME)
      AC_SUBST(ANDROID_NDK_HOME)
      AC_SUBST(ANDROID_SDK_VERSION)
      AC_SUBST(ANDROID_NDK_VERSION)
      AC_SUBST(ANDROID_GDBSERVER)
      
      ANDROID_BUILD=yes
      cross_compiling=yes
   fi
   
   AM_CONDITIONAL(GUI_ANDROID, [test "$ANDROID_BUILD" = "yes"])
        
])
