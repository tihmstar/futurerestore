#!/bin/zsh
echo 'step 1:'
set -e
export DIR=$(pwd)
echo "export PROCURSUS=/opt/procursus" >> ~/.bash_profile
echo "export PATH=${PROCURSUS}/bin:${PROCURSUS}/libexec/gnubin:${PATH}" >> ~/.bash_profile
echo "export PROCURSUS=/opt/procursus" >> ~/.zshrc
echo "export PATH=${PROCURSUS}/bin:${PROCURSUS}/libexec/gnubin:${PATH}" >> ~/.zshrc
export BASE=/Users/runner/work/futurerestore/futurerestore/.github/workflows
export PROCURSUS=/opt/procursus
export PATH=${PROCURSUS}/bin:${PROCURSUS}/libexec/gnubin:${PATH}
ssh-keyscan github.com >> ~/.ssh/known_hosts
echo 'step 2:'
zstd -dk bootstrap.tar.zst
sudo gtar xf ${BASE}/bootstrap.tar -C / --warning=none || true || true
sudo ${PROCURSUS}/bin/apt update -y
sudo ${PROCURSUS}/bin/apt install autopoint autoconf autoconf-archive automake bash bison cmake coreutils docbook-xml docbook-xsl dpkg fakeroot flex findutils gawk gnupg git grep groff ldid libtool make ncurses-bin openssl patch pkg-config po4a python3 sed tar triehash wget xz-utils zstd -y
echo 'step 3:'
cd ${BASE}/../..
export FUTURERESTORE_VERSION=$(git rev-parse HEAD | tr -d '\n')
export FUTURERESTORE_VERSION_RELEASE=$(cat version.txt | tr -d '\n')
echo 'step 4:'
git submodule init; git submodule update --recursive
cd external/tsschecker
git submodule init; git submodule update --recursive
cd ${BASE}
mkdir ${BASE}/Procursus
cd ${BASE}/Procursus
git init
git remote add origin https://github.com/ProcursusTeam/Procursus.git
git checkout -b main
git fetch origin 5455e273fe514f0055b16de3d32a0076546a9c5a
git reset --hard FETCH_HEAD
git apply ${BASE}/proc_ci.diff
gtar xf ${BASE}/build_base.tar
echo 'step 5:'
gmake futurerestore NO_PGP=1 MEMO_TARGET=darwin-amd64 MEMO_CFVER=1300
echo 'step 6:'
cp build_stage/darwin-amd64/1300/futurerestore/opt/procursus/bin/futurerestore ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE}
cd ${BASE}
gtar cpJvf ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE}-macOS.tar.xz futurerestore-${FUTURERESTORE_VERSION_RELEASE}
otool -L ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE} || true
${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE} || true
echo 'End'
