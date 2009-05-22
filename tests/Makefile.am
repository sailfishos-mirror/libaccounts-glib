## Process this file with automake to produce Makefile.in

INCLUDES=-I$(top_srcdir) $(LIBACCOUNTS_CFLAGS)

bin_PROGRAMS = accounts-glib-testsuite

accounts_glib_testsuite_SOURCES = check_ag.c
accounts_glib_testsuite_CFLAGS = $(CHECK_FLAGS) -I$(top_srcdir)
accounts_glib_testsuite_LDADD = \
	$(CHECK_LIBS) \
	$(LIBACCOUNTS_LIBS) \
	$(top_builddir)/libaccounts-glib/libaccounts-glib.la

TESTS_ENVIRONMENT = ACCOUNTS=/tmp/
TESTS = accounts-glib-testsuite

