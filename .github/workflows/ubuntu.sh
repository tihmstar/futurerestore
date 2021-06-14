#!/bin/bash
echo 'step 1:'
set -e
export LLVM_PATH=/home/runner/work/futurerestore/futurerestore/.github/llvm
export PATH=${LLVM_PATH}/bin:${PATH}
export DIR=$(pwd)
export BASE=/home/runner/work/futurerestore/futurerestore/.github/workflows
export CC_ARGS="CC=${LLVM_PATH}/bin/clang CXX=${LLVM_PATH}/bin/clang++ LD=${LLVM_PATH}/bin/ld64.lld RANLIB=/usr/bin/ranlib AR=/usr/bin/ar"
sudo ln -sf ${LLVM_PATH}/bin/ld64.lld /usr/bin/lld
sudo ln -sf ${LLVM_PATH}/bin/ld64.lld /usr/bin/ld
export CONF_ARGS="--disable-dependency-tracking --disable-silent-rules --prefix=/usr/local"
export LD_ARGS="-Wl,--allow-multiple-definition -L/usr/lib/x86_64-linux-gnu -lusb-1.0 -lzstd -llzma -lbz2 -L/usr/local/lib -lxpwn -lcommon -llzfse -lusbmuxd-2.0 -limg4tool -lgeneral -lirecovery-1.0 -lzip -lipatcher -loffsetfinder64 -limobiledevice-1.0 -lfragmentzip -linsn -lplist-2.0 -lplist++-2.0"
export JNUM="-j32"
sudo chown -R $USER:$USER /usr/local
sudo chown -R $USER:$USER /lib/udev/rules.d
cd ${BASE}
echo 'step 2:'
zstd -dk ubuntu.tar.zst
sudo tar xf ${BASE}/ubuntu.tar -C / --warning=none || true || true
sudo apt update -y
sudo apt install curl build-essential checkinstall git autoconf automake libtool-bin pkg-config cmake zlib1g-dev libssl-dev libbz2-dev libusb-1.0-0-dev libusb-dev libpng-dev libreadline-dev libcurl4-openssl-dev libzstd-dev liblzma-dev -y
curl -LO https://opensource.apple.com/tarballs/cctools/cctools-927.0.2.tar.gz
mkdir cctools-tmp
tar -xzf cctools-927.0.2.tar.gz -C cctools-tmp/
sed -i 's_#include_//_g' cctools-tmp/cctools-927.0.2/include/mach-o/loader.h
sed -i -e 's=<stdint.h>=\n#include <stdint.h>\ntypedef int integer_t;\ntypedef integer_t cpu_type_t;\ntypedef integer_t cpu_subtype_t;\ntypedef integer_t cpu_threadtype_t;\ntypedef int vm_prot_t;=g' cctools-tmp/cctools-927.0.2/include/mach-o/loader.h
sudo cp -LRP cctools-tmp/cctools-927.0.2/include/* /usr/local/include/
echo 'step 3:'
cd ${BASE}/../..
export FUTURERESTORE_VERSION_RELEASE=$(cat version.txt | tr -d '\n')
git submodule init; git submodule update --recursive
cd external/tsschecker
git submodule init; git submodule update --recursive
cd ${BASE}/../..
echo 'step 5:'
./autogen.sh $CONF_ARGS $CC_ARGS LDFLAGS="$LD_ARGS"
make $JNUM
sudo make $JNUM install
echo 'step 6:'
cp /usr/local/bin/futurerestore ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE}
cd ${BASE}
tar cpJvf ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE}-Ubuntu.tar.xz futurerestore-${FUTURERESTORE_VERSION_RELEASE}
ldd ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE} || true
./futurerestore-${FUTURERESTORE_VERSION_RELEASE} || true
echo 'End'
