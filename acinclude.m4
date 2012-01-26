# AC_CHECK_FASTCALL([if-ok],[if-notok])
# Test if the compiler recognizes __attribute__((regparm(3))) - we don't 
# currently check if it actually works correctly, but probably should...
# -----------------------
AC_DEFUN([AC_CHECK_FASTCALL], [
AC_MSG_CHECKING([support for fastcall calling conventions]);
AC_RUN_IFELSE([
  AC_LANG_SOURCE([[
int __attribute__((regparm(3))) foo(int a, int b) { return a+b; }

int main(int argc, char *argv[])
{
   return foo( 1, 2 ) == 3 ? 0 : 1;
}]])], [ 
   AC_MSG_RESULT([yes])
   $1 ], [ 
   AC_MSG_RESULT([no])
   $2 ], [
      AC_MSG_RESULT([no])
   $2 ])
])

# AC_CHECK_FORCEINLINE([if-ok],[if-notok])
# Test if the compiler recognizes __attribute__((always_inline))
# -----------------------
AC_DEFUN([AC_CHECK_FORCEINLINE], [
AC_MSG_CHECKING([support for force inlining]);
AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([[
static int __attribute__((always_inline)) foo(int a, int b) { return a+b; }

int main(int argc, char *argv[])
{
   return foo( 1, 2 ) == 3 ? 0 : 1;
}]])], [ 
   FORCEINLINE="__attribute__((always_inline))"
   AC_MSG_RESULT([$FORCEINLINE])
   $1 ], [ 
   FORCEINLINE=""
   AC_MSG_RESULT([no])
   $2 ])
])

# AC_CHECK_FRAME_ADDRESS([if-ok],[if-notok])
# Test if the compiler will let us modify the return address on the stack
# via __builtin_frame_address()
# -----------------------
AC_DEFUN([AC_CHECK_FRAME_ADDRESS], [
AC_MSG_CHECKING([if we have a working __builtin_frame_address()]);
AC_RUN_IFELSE([
  AC_LANG_SOURCE([[
void * __attribute__((noinline)) first_arg( void *x, void *y ) { return x; }
int __attribute__((noinline)) foo( int arg, void *exc )
{
    if( arg < 2 ) {
        *(((void * volatile *)__builtin_frame_address(0))+1) = exc;
    }
    return 0;
}

int main(int argc, char *argv[])
{
   goto *first_arg(&&start, &&except);
   
start:
   return foo( argc, &&except ) + 1;

except:
   return 0;
}]])], [ 
   AC_MSG_RESULT([yes])
   $1 ], [ 
   AC_MSG_RESULT([no])
   $2 ], [
   AC_MSG_RESULT([no])
   $2 ] )
])

# AC_CC_VERSION([if-gcc], [if-icc],[if-other])
# Check which C compiler we're using and branch accordingly, eg to set
# different optimization flags. Currently recognizes gcc and icc
# ---------------
AC_DEFUN([AC_CC_VERSION], [
_GCC_VERSION=`$CC --version | $SED -ne '/gcc/p'`
_ICC_VERSION=`$CC --version | $SED -ne '/(ICC)/p'`
AC_MSG_CHECKING([CC version])
if test -n "$_ICC_VERSION"; then
   AC_MSG_RESULT([ICC])
   [ $2 ]
elif test -n "$_GCC_VERSION"; then
   AC_MSG_RESULT([GCC])
   [ $1 ] 
else 
   AC_MSG_RESULT([Unknown])
   [ $3 ]
fi
]);

# AC_OBJC_VERSION([if-gcc],[if-other], [if-none])
# Check which objective C compiler we're using and branch accordingly.
AC_DEFUN([AC_OBJC_VERSION], [
AC_MSG_CHECKING([OBJC version])
if test -n "$OBJC"; then
  _GOBJC_VERSION=`$OBJC --version | $SED -ne '/(GCC)/p'`
  if test -n "$_GOBJC_VERSION"; then
    AC_MSG_RESULT([GCC])
    [ $1 ]
  else 
    AC_MSG_RESULT([Unknown])
    [ $2 ]
  fi
else
  AC_MSG_RESULT([None])
  [ $3 ]
fi
]);

# Check if the given C compiler flag is supported, and if so add it to CFLAGS
AC_DEFUN([AC_CHECK_CFLAG], [
AC_LANG_PUSH([C])
AC_MSG_CHECKING([if $CC supports $1])
save_CFLAGS="$CFLAGS"
CFLAGS="$1 $CFLAGS"
AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([int main() { return 0; }])], [
   AC_MSG_RESULT([yes])
   $2 ], [ 
   CFLAGS="$save_CFLAGS"
   AC_MSG_RESULT([no])
   $3 ])
AC_LANG_POP([C])
])

# Check if the given OBJC compiler flag is supported, and if so add it to OBJCFLAGS
AC_DEFUN([AC_CHECK_OBJCFLAG], [
AC_LANG_PUSH([Objective C])
AC_MSG_CHECKING([if $OBJC supports $1])
save_OBJCFLAGS="$OBJCFLAGS"
OBJCFLAGS="$1 $OBJCFLAGS"
AC_COMPILE_IFELSE([
  AC_LANG_SOURCE([int main() { return 0; }])], [
   AC_MSG_RESULT([yes])
   $2 ], [ 
   OBJCFLAGS="$save_OBJCFLAGS"
   AC_MSG_RESULT([no])
   $3 ])
AC_LANG_POP([Objective C])
])



# AC_HAVE_OBJC([if-present],[if-not-present])
# Check if we have a working Objective-C compiler
AC_DEFUN([AC_HAVE_OBJC], [
AC_PROG_OBJC
AC_MSG_CHECKING([for a working Objective-C compiler])
AC_LANG_PUSH([Objective C])dnl
_AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[@interface Foo @end]], [])],
   [AC_MSG_RESULT([yes])
    $1 ],
   [AC_MSG_RESULT([No])
    $2 ]);
AC_LANG_POP([Objective C])
]);



AC_DEFUN([AC_PROG_CC_FOR_BUILD], [dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_PROG_CPP])dnl
AC_REQUIRE([AC_EXEEXT])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
dnl
ac_main_cc="$CC"
test -n "$build_alias" && ac_build_tool_prefix=$build_alias-

pushdef([cross_compiling], [#])dnl
dnl If main compiler works and CC_FOR_BUILD is unset, use the main compiler
if test -z "$CC_FOR_BUILD"; then
    AC_RUN_IFELSE([int main(){return 0;}], [CC_FOR_BUILD="$CC"], [],[])
fi
dnl Use the standard macros, but make them use other variable names
dnl
pushdef([ac_cv_prog_CPP], ac_cv_build_prog_CPP)dnl
pushdef([ac_cv_prog_gcc], ac_cv_build_prog_gcc)dnl
pushdef([ac_cv_prog_cc_works], ac_cv_build_prog_cc_works)dnl
pushdef([ac_cv_prog_cc_cross], ac_cv_build_prog_cc_cross)dnl
pushdef([ac_cv_prog_cc_g], ac_cv_build_prog_cc_g)dnl
pushdef([ac_cv_exeext], ac_cv_build_exeext)dnl
pushdef([ac_cv_objext], ac_cv_build_objext)dnl
pushdef([ac_exeext], ac_build_exeext)dnl
pushdef([ac_objext], ac_build_objext)dnl
pushdef([CC], CC_FOR_BUILD)dnl
pushdef([CPP], CPP_FOR_BUILD)dnl
pushdef([CFLAGS], CFLAGS_FOR_BUILD)dnl
pushdef([CPPFLAGS], CPPFLAGS_FOR_BUILD)dnl
pushdef([host], build)dnl
pushdef([host_alias], build_alias)dnl
pushdef([host_cpu], build_cpu)dnl
pushdef([host_vendor], build_vendor)dnl
pushdef([host_os], build_os)dnl
pushdef([ac_tool_prefix], ac_build_tool_prefix)dnl
pushdef([ac_cv_host], ac_cv_build)dnl
pushdef([ac_cv_host_alias], ac_cv_build_alias)dnl
pushdef([ac_cv_host_cpu], ac_cv_build_cpu)dnl
pushdef([ac_cv_host_vendor], ac_cv_build_vendor)dnl
pushdef([ac_cv_host_os], ac_cv_build_os)dnl
pushdef([ac_cpp], ac_build_cpp)dnl
pushdef([ac_compile], ac_build_compile)dnl
pushdef([ac_link], ac_build_link)dnl

AC_PROG_CC
AC_PROG_CPP
AC_EXEEXT

dnl Restore the old definitions
dnl
popdef([ac_link])dnl
popdef([ac_compile])dnl
popdef([ac_cpp])dnl
popdef([ac_cv_host_os])dnl
popdef([ac_cv_host_vendor])dnl
popdef([ac_cv_host_cpu])dnl
popdef([ac_cv_host_alias])dnl
popdef([ac_cv_host])dnl
popdef([ac_tool_prefix])dnl
popdef([host_os])dnl
popdef([host_vendor])dnl
popdef([host_cpu])dnl
popdef([host_alias])dnl
popdef([host])dnl
popdef([CPPFLAGS])dnl
popdef([CFLAGS])dnl
popdef([CPP])dnl
popdef([CC])dnl
popdef([ac_objext])dnl
popdef([ac_exeext])dnl
popdef([ac_cv_objext])dnl
popdef([ac_cv_exeext])dnl
popdef([ac_cv_prog_cc_g])dnl
popdef([ac_cv_prog_cc_works])dnl
popdef([ac_cv_prog_cc_cross])dnl
popdef([ac_cv_prog_gcc])dnl
popdef([ac_cv_prog_CPP])dnl
popdef([cross_compiling])dnl

dnl Finally, set Makefile variables
dnl
BUILD_EXEEXT=$ac_build_exeext
BUILD_OBJEXT=$ac_build_objext
AC_SUBST(BUILD_EXEEXT)dnl
AC_SUBST(BUILD_OBJEXT)dnl
AC_SUBST([CFLAGS_FOR_BUILD])dnl
AC_SUBST([CPPFLAGS_FOR_BUILD])dnl
])
