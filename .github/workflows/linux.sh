#!/usr/bin/env bash
echo 'step 1:'
set -e
export DIR=$(pwd)
export BASE=/tmp/build/
export C_ARGS="-fPIC -static"
export CXX_ARGS="-fPIC -static"
export LD_ARGS="-Wl,--allow-multiple-definition -static -L/usr/lib/x86_64-linux-gnu -L/tmp/out/lib"
export C_ARGS2="-fPIC"
export CXX_ARGS2="-fPIC"
export LD_ARGS2="-Wl,--allow-multiple-definition -L/usr/lib/x86_64-linux-gnu -L/tmp/out/lib"
export PKG_CFG="/tmp/out/lib/pkgconfig"
export CC_ARGS="CC=/usr/bin/clang-13 CXX=/usr/bin/clang++-13 LD=/usr/bin/ld.lld-13 RANLIB=/usr/bin/ranlib AR=/usr/bin/ar"
export CONF_ARGS="--prefix=/tmp/out --disable-dependency-tracking --disable-silent-rules --disable-debug --without-cython --disable-shared"
export CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release -DCMAKE_CROSSCOMPILING=true -DCMAKE_C_FLAGS=${C_ARGS} -DCMAKE_CXX_FLAGS=${CXX_ARGS} -DCMAKE_SHARED_LINKER_FLAGS=${LD_ARGS} -DCMAKE_STATIC_LINKER_FLAGS=${LD_ARGS} -DCMAKE_INSTALL_PREFIX=/tmp/out -DBUILD_SHARED_LIBS=0 -Wno-dev"
export JNUM="-j$(($(nproc) / 2))"
export LNUM="-l$(($(nproc) / 2))"
cd ${BASE}
sed -i 's/deb\.debian\.org/ftp.de.debian.org/g' /etc/apt/sources.list
apt-get -qq update
apt-get -yqq dist-upgrade
apt-get install --no-install-recommends -yqq curl gnupg2 zstd lsb-release wget software-properties-common build-essential git autoconf automake libtool-bin pkg-config cmake zlib1g-dev libminizip-dev libpng-dev libreadline-dev libbz2-dev libudev-dev libudev1
curl -sO https://linux.cryptiiiic.com/CI-Scripts/linux.sh
chmod +x linux.sh
if [[ "$(file linux.sh)" == "linux.sh: Bourne-Again shell script, ASCII text executable" ]]
then 
    ./linux.sh
else
    cp -LRP /usr/bin/ld ~/
    rm -rf /usr/bin/ld /usr/lib/x86_64-linux-gnu/lib{usb-1.0,png*}.so*
    curl -sO https://apt.llvm.org/llvm.sh
    chmod 0755 llvm.sh
    ./llvm.sh 13
    ln -sf /usr/bin/ld.lld-13 /usr/bin/ld
    echo 'step 2:'
    curl -sO https://linux.cryptiiiic.com/CI-Scripts/linux.tar.zst
    zstd -dk linux.tar.zst
    tar xf ${BASE}/linux.tar -C / --warning=none || true || true
    echo 'step 3:'
    cd ${BASE}/futurerestore
    export FUTURERESTORE_VERSION_RELEASE=$(cat version.txt | tr -d '\n')
    git submodule init; git submodule update --recursive
    cd external/tsschecker
    git submodule init; git submodule update --recursive
    cd ${BASE}/futurerestore
    echo 'step 5:'
    ./autogen.sh ${CONF_ARGS} --enable-static ${CC_ARGS} CFLAGS="${C_ARGS2} -DIDEVICERESTORE_NOMAIN=1 -DTSSCHECKER_NOMAIN=1" LDFLAGS="${LD_ARGS2} -lpthread -ldl -lusb-1.0 -ludev -lusbmuxd-2.0 -llzfse -lcommon -lxpwn" PKG_CONFIG_PATH="${PKG_CFG}"
    make $JNUM $LNUM
    make $JNUM $LNUM install
    echo 'step 6:'
    cp /tmp/out/bin/futurerestore ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE}
    cd ${BASE}
    tar cpJvf ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE}-linux.tar.xz futurerestore-${FUTURERESTORE_VERSION_RELEASE}
    ldd ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE} || true
    ./futurerestore-${FUTURERESTORE_VERSION_RELEASE} || true
    echo 'End'
fi
