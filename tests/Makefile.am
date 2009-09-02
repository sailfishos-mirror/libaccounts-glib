## Process this file with automake to produce Makefile.in

INCLUDES=-I$(top_srcdir) $(LIBACCOUNTS_CFLAGS)

bin_PROGRAMS = accounts-glib-testsuite test-process

accounts_glib_testsuite_SOURCES = check_ag.c
accounts_glib_testsuite_CFLAGS = $(CHECK_FLAGS) -I$(top_srcdir)
accounts_glib_testsuite_LDADD = \
	$(CHECK_LIBS) \
	$(LIBACCOUNTS_LIBS) \
	$(top_builddir)/libaccounts-glib/libaccounts-glib.la

test_process_SOURCES = test-process.c
test_process_CFLAGS = -I$(top_srcdir)
test_process_LDADD = \
	$(LIBACCOUNTS_LIBS) \
	$(top_builddir)/libaccounts-glib/libaccounts-glib.la

TESTS_ENVIRONMENT = ACCOUNTS=/tmp/ AG_SERVICES=$(top_srcdir)/tests/
TESTS = accounts-glib-testsuite

EXTRA_DIST = \
	MyService.service \
	OtherService.service

