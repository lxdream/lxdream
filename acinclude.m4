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


