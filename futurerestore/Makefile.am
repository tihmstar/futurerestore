AM_CFLAGS = $(libplist_CFLAGS) $(libzip_CFLAGS) $(libimobiledevice_CFLAGS) $(libfragmentzip_CFLAGS) $(libirecovery_CFLAGS) -I$(top_srcdir)/external/tsschecker/external/jssy/jssy -I$(top_srcdir)/external/tsschecker/tsschecker -I$(top_srcdir)/external/img4tool/img4tool -I$(top_srcdir)/external/idevicerestore/src -std=c++11
AM_LDFLAGS = $(libplist_LIBS) $(libzip_LIBS) $(libimobiledevice_LIBS) $(libfragmentzip_LIBS) $(libirecovery_LIBS) 

if HAVE_LIBIPATCHER
AM_LDFLAGS += $(libipatcher_LIBS)
AM_CFLAGS += $(libipatcher_CFLAGS) -DHAVE_LIBIPATCHER
endif

bin_PROGRAMS	= futurerestore
futurerestore_CXXFLAGS = $(AM_CFLAGS)
futurerestore_LDADD = $(top_srcdir)/external/idevicerestore/src/libidevicerestore.la $(top_srcdir)/external/img4tool/img4tool/libimg4tool.la $(top_srcdir)/external/tsschecker/tsschecker/libtsschecker.la $(top_srcdir)/external/tsschecker/tsschecker/libjssy.a $(AM_LDFLAGS)
futurerestore_SOURCES = futurerestore.cpp main.cpp
