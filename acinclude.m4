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

