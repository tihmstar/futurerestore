#!/usr/bin/env bash

set -e
export TMPDIR=/tmp
export BASE=${TMPDIR}/Builder/repos/futurerestore/.github/workflows

#sed -i 's/deb\.debian\.org/ftp.de.debian.org/g' /etc/apt/sources.list
apt-get -qq update
apt-get -yqq dist-upgrade
apt-get install --no-install-recommends -yqq zstd curl gnupg2 lsb-release wget software-properties-common build-essential git autoconf automake libtool-bin pkg-config cmake zlib1g-dev libminizip-dev libpng-dev libreadline-dev libbz2-dev libudev-dev libudev1
cp -RpP /usr/bin/ld /
rm -rf /usr/bin/ld /usr/lib/x86_64-linux-gnu/lib{usb-1.0,png*,readline}.so*
cd ${TMPDIR}/Builder/repos/futurerestore
git submodule update --init --recursive
cd ${BASE}
curl -sO https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
./llvm.sh 13 all
ln -sf /usr/bin/ld.lld-13 /usr/bin/ld
curl -sO https://cdn.cryptiiiic.com/bootstrap/Builder_Linux.tar.zst &
curl -sO https://cdn.cryptiiiic.com/deps/static/Linux/x86_64/Linux_x86_64_Release_Latest.tar.zst &
curl -sO https://cdn.cryptiiiic.com/deps/static/Linux/x86_64/Linux_x86_64_Debug_Latest.tar.zst &
wait
tar xf Linux_x86_64_Release_Latest.tar.zst -C ${TMPDIR}/Builder &
tar xf Linux_x86_64_Debug_Latest.tar.zst -C ${TMPDIR}/Builder &
tar xf Builder_Linux.tar.zst &
wait
rm -rf "*.zst"
cd ${BASE}
