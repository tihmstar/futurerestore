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
AC_SUBST([VERSION_COMMIT_COUNT], ["m4_esyscmd([git rev-list --count HEAD | tr -d '\n'])"])
AC_SUBST([VERSION_COMMIT_SHA], ["m4_esyscmd([git rev-parse HEAD | tr -d '\n'])"])

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
CXXFLAGS+=" -std=c++17"
CFLAGS+=" -std=c17"

AC_DEFUN([AX_UNPRECIOUS], [
    m4_define([_AC_PRECIOUS_VARS], m4_bpatsubst(_AC_PRECIOUS_VARS, [$1], []))
])
AX_UNPRECIOUS([CFLAGS])
export CFLAGS

AC_CONFIG_SRCDIR([futurerestore])

# Checks the libraries.
LIBPLIST_REQUIRES_STR="libplist-2.0 >= 2.3.0"
LIBZIP_REQUIRES_STR="libzip >= 0.10"
LIBIMOBILEDEVICE_REQUIRES_STR="libimobiledevice-1.0 >= 1.2.1"
LIBFRAGMENTZIP_REQUIRES_STR="libfragmentzip >= 47"
LIBFWKEYFETCH_REQUIRES_STR="libfwkeyfetch >= 1"
LIBIRECOVERY_REQUIRES_STR="libirecovery-1.0 >= 1.0.0"
IMG4TOOL_REQUIRES_STR="libimg4tool >= 162"
LIBGENERAL_REQUIRES_STR="libgeneral >= 26"
LIBIDEVICERESTORE_REQUIRES_STR="libidevicerestore >= 1.0.0"
LIBRA1NSN0W_REQUIRES_STR="libra1nsn0w >= 158"

PKG_CHECK_MODULES(libplist, $LIBPLIST_REQUIRES_STR)
PKG_CHECK_MODULES(libzip, $LIBZIP_REQUIRES_STR)
PKG_CHECK_MODULES(libimobiledevice, $LIBIMOBILEDEVICE_REQUIRES_STR)
PKG_CHECK_MODULES(libfragmentzip, $LIBFRAGMENTZIP_REQUIRES_STR)
PKG_CHECK_MODULES(libirecovery, $LIBIRECOVERY_REQUIRES_STR)
PKG_CHECK_MODULES(libimg4tool, $IMG4TOOL_REQUIRES_STR)
PKG_CHECK_MODULES(libgeneral, $LIBGENERAL_REQUIRES_STR)
PKG_CHECK_MODULES(libidevicerestore, $LIBIDEVICERESTORE_REQUIRES_STR)
PKG_CHECK_MODULES(libra1nsn0w, $LIBRA1NSN0W_REQUIRES_STR)
PKG_CHECK_MODULES(libfwkeyfetch, $LIBFWKEYFETCH_REQUIRES_STR)

LT_INIT

AC_OUTPUT([
Makefile
futurerestore/Makefile
])

echo "
Configuration for $PACKAGE $VERSION:
-------------------------------------------

  Install prefix ..........: $prefix
  Now type 'make' to build $PACKAGE $VERSION,
  and then 'make install' for installation.
"
