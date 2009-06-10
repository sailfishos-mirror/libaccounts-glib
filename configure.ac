dnl Process this file with autoconf to produce a configure script.

AC_INIT(libaccounts-glib, 0.5)

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config.h)

PKG_CHECK_MODULES(
	LIBACCOUNTS,
	dbus-1
	glib-2.0
	gobject-2.0
	libxml-2.0
	sqlite3);
AC_SUBST(LIBACCOUNTS_CFLAGS)
AC_SUBST(LIBACCOUNTS_LIBS)

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_LIBTOOL

GTK_DOC_CHECK([1.3])

AC_ARG_ENABLE(cast-checks,  [  --disable-cast-checks   compile with GLIB cast checks disabled],[cchecks=${enableval}],cchecks=yes)
if test "x$cchecks" = "xno"; then
    CFLAGS="$CFLAGS -DG_DISABLE_CAST_CHECKS"
fi

AC_ARG_ENABLE(asserts,      [  --disable-asserts       compile with GLIB assertions disabled],[asserts=${enableval}],asserts=yes)
if test "x$asserts" = "xno"; then
    CFLAGS="$CFLAGS -DG_DISABLE_ASSERTS"
fi

AC_ARG_ENABLE(checks,       [  --disable-checks        compile with GLIB checks disabled],[checks=${enableval}],checks=yes) 
if test "x$checks" = "xno"; then
    CFLAGS="$CFLAGS -DG_DISABLE_CHECKS"
fi

AC_ARG_ENABLE(debug,     [  --enable-debug          enable printing of debug messages],[ddebug=${enableval}],ddebug=no)
if test "x$ddebug" != "xyes"; then
    CFLAGS="$CFLAGS -DG_DEBUG_DISABLE"
fi

AC_ARG_ENABLE(coverage, [  --enable-coverage      compile with coverage info],[coverage=${enableval}],coverage=no)
if test "x$coverage" = "xyes"; then
    CFLAGS="$CFLAGS -g -fprofile-arcs -ftest-coverage"
fi

AC_OUTPUT([
	Makefile
	libaccounts-glib/Makefile
	libaccounts-glib.pc
	docs/Makefile
	docs/reference/Makefile
	tests/Makefile
])
