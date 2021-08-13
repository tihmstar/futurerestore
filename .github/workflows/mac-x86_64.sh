#!/usr/bin/env zsh
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
curl -so - https://mac.cryptiiiic.com/public.gpg | sudo apt-key add -
echo "Types: deb\nURIs: https://mac.cryptiiiic.com/\nSuites: x86_64\nComponents: main\n" | sudo tee -a /opt/procursus/etc/apt/sources.list.d/cryptic.sources
sudo ${PROCURSUS}/bin/apt update -y
sudo ${PROCURSUS}/bin/apt install autopoint autoconf autoconf-archive automake bash bison cmake coreutils docbook-xml docbook-xsl dpkg fakeroot flex findutils gawk gnupg git grep groff ldid libtool make ncurses-bin openssl patch pkg-config po4a python3 sed tar triehash wget xz-utils zstd fd -y
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
mkdir /Users/runner/Procursus
cd /Users/runner/Procursus
touch .keep
git init
git remote add origin https://github.com/ProcursusTeam/Procursus.git
git checkout -b main
git fetch origin fea8e5c1bbe12405051b765fb696a86e4aed2fa4
git reset --hard FETCH_HEAD
git apply ${BASE}/proc_ci.diff
sudo ${PROCURSUS}/bin/apt install libgeneral-proc libimg4tool-proc libimobiledevice-proc libinsn-proc libipatcher-proc libirecovery-proc liboffsetfinder64-proc libplist-proc libpng16-proc libssl-proc libusbmuxd-proc libxpwn-proc libzip-proc
sudo chown -R $(id -u):$(id -g) /Users/runner/Procursus
echo 'step 5:'
gmake futurerestore-package NO_PGP=1 MEMO_TARGET=darwin-amd64 MEMO_CFVER=1300 DEBUG=0
echo 'step 6:'
rm -rf build_stage/darwin-amd64/1300/futurerestore/*
dpkg -X build_dist/darwin-amd64/1300/futurerestore*.deb build_stage/darwin-amd64/1300/futurerestore
cp -v build_stage/darwin-amd64/1300/futurerestore/opt/procursus/bin/futurerestore ${BASE}/futurerestore-x86_64-${FUTURERESTORE_VERSION_RELEASE}
cd ${BASE}
otool -L ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE} || true
gtar cpJvf ${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE}-macOS-x86_64.tar.xz futurerestore-x86_64-${FUTURERESTORE_VERSION_RELEASE}
${BASE}/futurerestore-${FUTURERESTORE_VERSION_RELEASE} || true
echo 'End'
