#!/usr/bin/env bash

set -e
export TMPDIR=/tmp
export WORKFLOW_ROOT=${TMPDIR}/Builder/repos/futurerestore/.github/workflows
export DEP_ROOT=${TMPDIR}/Builder/repos/futurerestore/dep_root
export BASE=${TMPDIR}/Builder/repos/futurerestore/

#sed -i 's/deb\.debian\.org/ftp.de.debian.org/g' /etc/apt/sources.list
apt-get -qq update
apt-get -yqq dist-upgrade
apt-get install --no-install-recommends -yqq zstd curl gnupg2 lsb-release wget software-properties-common build-essential git autoconf automake libtool-bin pkg-config cmake zlib1g-dev libminizip-dev libpng-dev libreadline-dev libbz2-dev libudev-dev libudev1
cp -RpP /usr/bin/ld /
rm -rf /usr/bin/ld /usr/lib/x86_64-linux-gnu/lib{usb-1.0,png*,readline}.so*
cd ${TMPDIR}/Builder/repos/futurerestore
git submodule update --init --recursive
cd ${WORKFLOW_ROOT}
curl -sO https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
./llvm.sh 13 all
ln -sf /usr/bin/ld.lld-13 /usr/bin/ld
curl -sO https://cdn.cryptiiiic.com/bootstrap/linux_fix.tar.zst &
curl -sO https://cdn.cryptiiiic.com/deps/static/Linux/x86_64/Linux_x86_64_Release_Latest.tar.zst &
curl -sO https://cdn.cryptiiiic.com/deps/static/Linux/x86_64/Linux_x86_64_Debug_Latest.tar.zst &
curl -sLO https://github.com/Kitware/CMake/releases/download/v3.23.2/cmake-3.23.2-linux-x86_64.tar.gz &
wait
rm -rf ${DEP_ROOT}/{lib,include} || true
mkdir -p ${DEP_ROOT}/Linux_x86_64_{Release,Debug}
tar xf Linux_x86_64_Release_Latest.tar.zst -C ${DEP_ROOT}/Linux_x86_64_Release &
tar xf Linux_x86_64_Debug_Latest.tar.zst -C ${DEP_ROOT}/Linux_x86_64_Debug &
tar xf linux_fix.tar.zst -C ${TMPDIR}/Builder &
tar xf cmake-3.23.2-linux-x86_64.tar.gz
cp -RpP cmake-3.23.2-linux-x86_64/* /usr/local/ || true
wait
rm -rf "*.zst"
cd ${WORKFLOW_ROOT}
