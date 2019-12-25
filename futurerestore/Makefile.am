AM_CFLAGS = -I$(top_srcdir)/external/libgeneral/include -I$(top_srcdir)/external/tsschecker/external/jssy/jssy -I$(top_srcdir)/external/tsschecker/tsschecker -I$(top_srcdir)/external/idevicerestore/src $(libplist_CFLAGS) $(libzip_CFLAGS) $(libimobiledevice_CFLAGS) $(libfragmentzip_CFLAGS) $(libirecovery_CFLAGS) $(libimg4tool_CFLAGS) $(libgeneral_CFLAGS)
AM_LDFLAGS = $(libplist_LIBS) $(libzip_LIBS) $(libimobiledevice_LIBS) $(libfragmentzip_LIBS) $(libirecovery_LIBS) $(libimg4tool_LIBS) $(libgeneral_LIBS)

if HAVE_LIBIPATCHER
AM_LDFLAGS += $(libipatcher_LIBS)
AM_CFLAGS += $(libipatcher_CFLAGS)
endif

bin_PROGRAMS = futurerestore
futurerestore_CXXFLAGS = $(AM_CFLAGS)
futurerestore_LDADD = $(top_srcdir)/external/idevicerestore/src/libidevicerestore.la  $(top_srcdir)/external/tsschecker/tsschecker/libtsschecker.la $(top_srcdir)/external/tsschecker/tsschecker/libjssy.a $(AM_LDFLAGS)
futurerestore_SOURCES = futurerestore.cpp main.cpp
