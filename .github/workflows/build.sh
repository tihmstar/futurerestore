#!/bin/zsh
echo 'step 1:'
set -e
export DIR=$(pwd) SR=/usr/local/SYSROOT HOMEBREW_NO_INSTALL_CLEANUP=1 HOMEBREW_NO_INSTALLED_DEPENDENTS_CHECK=1 HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_MAKE_JOBS=16 export BASE=/Users/runner/work/futurerestore/futurerestore/.github/workflows HB=/usr/local/Homebrew/Library/Taps/homebrew/homebrew-core/Formula CC='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang' CXX='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++' LD='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ld' RANLIB='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ranlib' AR='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ar' CFLAGS='-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -mmacosx-version-min=10.15' CXXFLAGS='-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -mmacosx-version-min=10.15'
brew install --force make cmake autoconf automake
echo 'step 2:'
cd $BASE/../..
git fetch origin test
git reset --hard FETCH_HEAD
echo 'step 3:'
git submodule init; git submodule update --recursive
cd external/tsschecker
git submodule init; git submodule update --recursive
cd ../../
find /usr/local/Cellar -type f \( -iname "*.a" ! -iname  "libcrypto.a" ! -iname "libssl.a" ! -iname "libzstd.a" ! -iname "liblzma.a" ! -iname "liblzma.a" ! -iname "libzip.a" ! -iname "libusb-1.0.a" ! -iname "libplist-2.0.a" ! -iname "libplist++-2.0.a" ! -iname "libusbmuxd-2.0.a" ! -iname "libimobiledevice-1.0.a" ! -iname "libirecovery-1.0.a" ! -iname "libgeneral.a" ! -iname "libinsn.a" ! -iname "liboffsetfinder64.a" ! -iname "libfragmentzip.a" ! -iname "libimg4tool.a" ! -iname "libjssy.a" ! -iname "libiBoot32Patcher.a" ! -iname "libipatcher.a" ! -iname "libcommon.a" ! -iname "libxpwn.a" ! -iname "libpng16.*a" \)
find /usr/local/opt -type f \( -iname "*.a" ! -iname  "libcrypto.a" ! -iname "libssl.a" ! -iname "libzstd.a" ! -iname "liblzma.a" ! -iname "liblzma.a" ! -iname "libzip.a" ! -iname "libusb-1.0.a" ! -iname "libplist-2.0.a" ! -iname "libplist++-2.0.a" ! -iname "libusbmuxd-2.0.a" ! -iname "libimobiledevice-1.0.a" ! -iname "libirecovery-1.0.a" ! -iname "libgeneral.a" ! -iname "libinsn.a" ! -iname "liboffsetfinder64.a" ! -iname "libfragmentzip.a" ! -iname "libimg4tool.a" ! -iname "libjssy.a" ! -iname "libiBoot32Patcher.a" ! -iname "libipatcher.a" ! -iname "libcommon.a" ! -iname "libxpwn.a" ! -iname "libpng16.*a" \)
find /usr/local/lib -type f \( -iname "*.a" ! -iname  "libcrypto.a" ! -iname "libssl.a" ! -iname "libzstd.a" ! -iname "liblzma.a" ! -iname "liblzma.a" ! -iname "libzip.a" ! -iname "libusb-1.0.a" ! -iname "libplist-2.0.a" ! -iname "libplist++-2.0.a" ! -iname "libusbmuxd-2.0.a" ! -iname "libimobiledevice-1.0.a" ! -iname "libirecovery-1.0.a" ! -iname "libgeneral.a" ! -iname "libinsn.a" ! -iname "liboffsetfinder64.a" ! -iname "libfragmentzip.a" ! -iname "libimg4tool.a" ! -iname "libjssy.a" ! -iname "libiBoot32Patcher.a" ! -iname "libipatcher.a" ! -iname "libcommon.a" ! -iname "libxpwn.a" ! -iname "libpng16.*a" \)
sudo find /usr/local/lib -name 'libzstd*.dylib' -delete
sudo find /usr/local/lib -name 'libzstd*'
sudo find /usr/local/lib -iname 'libzip*.dylib' -delete
sudo find /usr/local/lib -iname 'liblzma*.dylib' -delete
sudo find /usr/local/lib -iname 'libusb-1.0*.dylib' -delete
sudo find /usr/local/lib -iname 'libpng*.dylib' -delete
./autogen.sh --disable-dependency-tracking --disable-silent-rules --prefix=/usr/local --disable-debug CC='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang' CXX='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++' LD='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ld' RANLIB='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ranlib' AR='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/ar' CFLAGS='-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -mmacosx-version-min=10.15 -DTSSCHECKER_NOMAIN=1 -DIDEVICERESTORE_NOMAIN=1' CXXFLAGS='-isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -mmacosx-version-min=10.15 -DTSSCHECKER_NOMAIN=1 -DIDEVICERESTORE_NOMAIN=1' LDFLAGS='-L/usr/local/lib -lzstd -lusbmuxd-2.0 -L/usr/lib -lbz2 -llzma -lcompression  -framework CoreFoundation -framework IOKit'
echo 'step 4:'
gmake -j16
echo 'step 5:'
gmake -j16 install
echo 'step 6:'
/usr/local/bin/futurerestore || true
echo 'step 7:'
otool -L /usr/local/bin/futurerestore
echo 'step 8:'
mv /usr/local/bin/futurerestore $BASE/futurerestore
echo 'End'
