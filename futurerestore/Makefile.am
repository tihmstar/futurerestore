AM_CFLAGS = $(libplist_CFLAGS) $(libzip_CFLAGS) $(libimobiledevice_CFLAGS) $(libfragmentzip_CFLAGS) $(libfwkeyfetch_CFLAGS) \
						$(libirecovery_CFLAGS) $(libimg4tool_CFLAGS) $(libgeneral_CFLAGS) $(libidevicerestore_CFLAGS) $(libra1nsn0w_CFLAGS)
AM_LDFLAGS = 	$(libplist_LIBS) $(libzip_LIBS) $(libimobiledevice_LIBS) $(libfragmentzip_LIBS) $(libfwkeyfetch_LIBS) \
							$(libirecovery_LIBS) $(libimg4tool_LIBS) $(libgeneral_LIBS) $(libidevicerestore_LIBS) $(libra1nsn0w_LIBS)

bin_PROGRAMS = futurerestore
futurerestore_CXXFLAGS = $(AM_CFLAGS)
futurerestore_LDADD =  $(AM_LDFLAGS)
futurerestore_SOURCES = futurerestore.cpp main.cpp
