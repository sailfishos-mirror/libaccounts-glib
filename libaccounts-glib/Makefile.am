lib_LTLIBRARIES = \
	libaccounts-glib.la

libaccounts_glib_la_CFLAGS = $(LIBACCOUNTS_CFLAGS) -Wall -Werror
libaccounts_glib_la_LIBADD = $(LIBACCOUNTS_LIBS)
libaccounts_glib_la_LDFLAGS = -version-info 0:0:0

BUILT_SOURCES = \
	account-marshal.c \
	account-marshal.h

CLEANFILES = $(BUILT_SOURCES)

libaccounts_glib_la_SOURCES = \
	account-item.h \
	account-item.c \
	account-manager.h \
	account-manager.c \
	$(BUILT_SOURCES)

libaccounts_glib_includedir = $(includedir)/libaccounts-glib
libaccounts_glib_include_HEADERS = \
	account-item.h \
	account-manager.h

%-marshal.h: %-marshal.list Makefile
	glib-genmarshal --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile
	glib-genmarshal --body --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.c


EXTRA_DIST = account-marshal.list

