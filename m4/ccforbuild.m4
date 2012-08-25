AC_DEFUN([AC_PUSH_CC_FOR_BUILD], [dnl
pushdef([ac_cv_prog_CPP], ac_cv_build_prog_CPP)dnl
pushdef([ac_cv_prog_gcc], ac_cv_build_prog_gcc)dnl
pushdef([ac_cv_prog_cc_works], ac_cv_build_prog_cc_works)dnl
pushdef([ac_cv_prog_cc_cross], ac_cv_build_prog_cc_cross)dnl
pushdef([ac_cv_prog_cc_g], ac_cv_build_prog_cc_g)dnl
pushdef([ac_cv_prog_cc_stdc], ac_cv_build_prog_cc_stdc)dnl
pushdef([ac_cv_prog_cc_c99], ac_cv_build_prog_cc_stdc)dnl
pushdef([ac_cv_prog_cc_c89], ac_cv_build_prog_cc_stdc)dnl
dnl pushdef([ac_cv_c_compiler_gnu], ac_cv_build_c_compiler_gnu)dnl
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
pushdef([ac_compiler_gnu], ac_build_compiler_gnu)dnl
pushdef([ac_tool_prefix], ac_build_tool_prefix)dnl
pushdef([ac_cv_host], ac_cv_build)dnl
pushdef([ac_cv_host_alias], ac_cv_build_alias)dnl
pushdef([ac_cv_host_cpu], ac_cv_build_cpu)dnl
pushdef([ac_cv_host_vendor], ac_cv_build_vendor)dnl
pushdef([ac_cv_host_os], ac_cv_build_os)dnl
pushdef([ac_cpp], ac_build_cpp)dnl
pushdef([ac_compile], ac_build_compile)dnl
pushdef([ac_link], ac_build_link)dnl
])

AC_DEFUN([AC_POP_CC_FOR_BUILD], [dnl
popdef([ac_link])dnl
popdef([ac_compile])dnl
popdef([ac_cpp])dnl
popdef([ac_cv_host_os])dnl
popdef([ac_cv_host_vendor])dnl
popdef([ac_cv_host_cpu])dnl
popdef([ac_cv_host_alias])dnl
popdef([ac_cv_host])dnl
popdef([ac_tool_prefix])dnl
popdef([ac_compiler_gnu])dnl
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
dnl popdef([ac_cv_c_compiler_gnu])dnl
popdef([ac_cv_prog_cc_c89])dnl
popdef([ac_cv_prog_cc_c99])dnl
popdef([ac_cv_prog_cc_stdc])dnl
popdef([ac_cv_prog_cc_g])dnl
popdef([ac_cv_prog_cc_works])dnl
popdef([ac_cv_prog_cc_cross])dnl
popdef([ac_cv_prog_gcc])dnl
popdef([ac_cv_prog_CPP])dnl
])


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
AC_PUSH_CC_FOR_BUILD

AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_CPP
AC_EXEEXT

dnl Restore the old definitions
dnl
AC_POP_CC_FOR_BUILD
popdef([cross_compiling])dnl
dnl Finally, set Makefile variables
dnl
BUILD_EXEEXT=$ac_build_exeext
BUILD_OBJEXT=$ac_build_objext
AC_SUBST(BUILD_EXEEXT)dnl
AC_SUBST(BUILD_OBJEXT)dnl
AC_SUBST([CFLAGS_FOR_BUILD])dnl
AC_SUBST([CPPFLAGS_FOR_BUILD])dnl
AC_SUBST([LDFLAGS_FOR_BUILD])dnl
])
