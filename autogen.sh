#!/bin/bash
gprefix=`which glibtoolize 2>&1 >/dev/null`
if [ $? -eq 0 ]; then
  glibtoolize --force
else
  libtoolize --force
fi
aclocal -I m4
autoconf
autoheader
automake --add-missing
autoreconf -i

export NOCONFIGURE=1

SUBDIRS="external/idevicerestore external/tsschecker"
for SUB in $SUBDIRS; do
    pushd $SUB
    ./autogen.sh
    popd
done

unset NOCONFIGURE

if [ -z "$NOCONFIGURE" ]; then
    ./configure "$@"
fi
