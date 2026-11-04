#!/bin/bash

#cleanup cache for correct versioning when run multiple times
rm -rf autom4te.cache


aclocal -I m4
autoconf
autoheader
automake --add-missing
autoreconf -i
./configure "$@"
