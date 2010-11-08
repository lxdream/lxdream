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
        *(((void **)__builtin_frame_address(0))+1) = exc;
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
   $2 ])
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
