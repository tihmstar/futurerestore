AC_PREREQ([2.69])
AC_INIT([futurerestore], m4_esyscmd([git rev-list --count HEAD | tr -d '\n']), [tihmstar@gmail.com])

AC_CANONICAL_SYSTEM
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

# Versioning.
AC_DEFINE([VERSION_COMMIT_COUNT], "m4_esyscmd([git rev-list --count HEAD | tr -d '\n'])", [Git commit count])
AC_DEFINE([VERSION_COMMIT_SHA], "m4_esyscmd([git rev-parse HEAD | tr -d '\n'])", [Git commit sha])
AC_DEFINE([VERSION_RELEASE], "m4_esyscmd([cat version.txt | tr -d '\n'])", [Futurerestore release version])
AC_SUBST([VERSION_COMMIT_COUNT], ["m4_esyscmd([git rev-list --count HEAD | tr -d '\n'])"])
AC_SUBST([VERSION_COMMIT_SHA], ["m4_esyscmd([git rev-parse HEAD | tr -d '\n'])"])
AC_SUBST([VERSION_RELEASE], ["m4_esyscmd([cat version.txt | tr -d '\n'])"])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Check for operating system.
AC_MSG_CHECKING([whether we need platform-specific build settings])
case $host_os in
  darwin* )
        CXXFLAGS+=" -stdlib=libc++"
        ;;
esac

# Checks the flags.
CFLAGS+=" -DIDEVICERESTORE_NOMAIN=1 -DTSSCHECKER_NOMAIN=1"
CXXFLAGS+=" -std=gnu++11"
CFLAGS+=" -std=gnu11"

AC_DEFUN([AX_UNPRECIOUS], [
    m4_define([_AC_PRECIOUS_VARS], m4_bpatsubst(_AC_PRECIOUS_VARS, [$1], []))
])
AX_UNPRECIOUS([CFLAGS])
export CFLAGS

AC_CONFIG_SUBDIRS(external/idevicerestore external/tsschecker)
AC_CONFIG_SRCDIR([futurerestore])

# Checks the libraries.
LIBIPATCHER_REQUIRES_STR="libipatcher >= 61"
LIBPLIST_REQUIRES_STR="libplist-2.0 >= 2.2.0"
LIBZIP_REQUIRES_STR="libzip >= 0.10"
LIBIMOBILEDEVICE_REQUIRES_STR="libimobiledevice-1.0 >= 1.2.1"
LIBFRAGMENTZIP_REQUIRES_STR="libfragmentzip >= 47"
LIBIRECOVERY_REQUIRES_STR="libirecovery-1.0 >= 1.0.0"
IMG4TOOL_REQUIRES_STR="libimg4tool >= 162"
LIBGENERAL_REQUIRES_STR="libgeneral >= 26"

PKG_CHECK_MODULES(libplist, $LIBPLIST_REQUIRES_STR)
PKG_CHECK_MODULES(libzip, $LIBZIP_REQUIRES_STR)
PKG_CHECK_MODULES(libimobiledevice, $LIBIMOBILEDEVICE_REQUIRES_STR)
PKG_CHECK_MODULES(libfragmentzip, $LIBFRAGMENTZIP_REQUIRES_STR)
PKG_CHECK_MODULES(libirecovery, $LIBIRECOVERY_REQUIRES_STR)
PKG_CHECK_MODULES(libimg4tool, $IMG4TOOL_REQUIRES_STR)
PKG_CHECK_MODULES(libgeneral, $LIBGENERAL_REQUIRES_STR)

# Optional module libipatcher
AC_ARG_WITH([libipatcher],
            [AS_HELP_STRING([--without-libipatcher],
            [build with bundle-less odysseus support (default is yes)])],
            [build_libipatcher=false],
            [build_libipatcher=true])

PKG_PROG_PKG_CONFIG
LIBIPATCHER_FLAGS=$($PKG_CONFIG --cflags libipatcher)
if [test -z "$LIBIPATCHER_FLAGS"]; then
    do_libipatcher=no
else
    AC_MSG_CHECKING([for futurerestore libipatcher])
    if test "$build_libipatcher" = "true"; then
            PKG_CHECK_MODULES(libipatcher, $LIBIPATCHER_REQUIRES_STR)
            AC_DEFINE(HAVE_LIBIPATCHER, 1, [Define if you have libipatcher])
            do_libipatcher=yes
    else
            do_libipatcher=no
    fi
fi
AM_CONDITIONAL([HAVE_LIBIPATCHER],[test "x$do_libipatcher" = "xyes"])

AC_DEFINE(CUSTOM_LOGGING, <stdlib.h>, [required for futurerestore])

LT_INIT

AC_OUTPUT([
Makefile
futurerestore/Makefile
])

echo "
Configuration for $PACKAGE $VERSION:
-------------------------------------------

  Install prefix ..........: $prefix
  With libipatcher ........: $do_libipatcher
  Now type 'make' to build $PACKAGE $VERSION,
  and then 'make install' for installation.
"
