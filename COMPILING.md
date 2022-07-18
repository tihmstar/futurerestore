# Compiling Futurerestore

# Required/Optional Dependencies
* ## External libs
  Make sure these are installed
    * [curl](https://github.com/curl/curl) (Linux/Windows only, macOS already has curl preinstalled);
    * [openssl 3.0.3](https://github.com/openssl/openssl) (or CommonCrypto on macOS);
    * [libusb 1.0.25](https://github.com/libusb/libusb) (Linux/Windows only, macOS can use IOKit for libirecovery);
    * [libzip](https://github.com/nih-at/libzip);
    * [libplist](https://github.com/libimobiledevice/libplist);
    * [libusbmuxd](https://github.com/libimobiledevice/libusbmuxd);
    * [libirecovery](https://github.com/libimobiledevice/libirecovery);
    * [libimobiledevice](https://github.com/libimobiledevice/libimobiledevice);
    * [libimobiledevice-glue](https://github.com/libimobiledevice/libimobiledevice-glue);
    * [libpng16](https://github.com/glennrp/libpng);
    * [xpwn(fork)](https://github.com/nyuszika7h/xpwn);
    * [libgeneral](https://github.com/tihmstar/libgeneral);
    * [libfragmentzip](https://github.com/tihmstar/libfragmentzip);
    * [libinsn](https://github.com/tihmstar/libinsn);
    * [lzfse](https://github.com/lzfse/lzfse) (Linux/Windows only, macOS has built in libcompression);
    * [img4tool](https://github.com/tihmstar/img4tool);
    * [liboffsetfinder64(fork))](https://github.com/Cryptiiiic/liboffsetfinder64);
    * [libipatcher(fork)](https://github.com/Cryptiiiic/libipatcher)

* ## Submodules
  Make sure these projects compile on your system (install it's dependencies):

    * [tsschecker(fork)](https://github.com/1Conan/tsschecker);
    * * [jssy](https://github.com/tihmstar/jssy) (This is a submodule of tsschecker);
    * [idevicerestore(fork)](https://github.com/futurerestore/idevicerestore)

  If you are cloning this repository you may run:

  ```git clone https://github.com/futurerestore/futurerestore --recursive```

  which will clone these submodules for you.

# Building from source
* ## dep_root
  After obtaining all the required/optional dependencies you can either install them to your

  system for building dynamically or place the static libs in `dep_root/lib` and headers in

  `dep_root/include` for building statically
* ## build.sh
  Executing build.sh will configure and building in debug mode by default and an arch must be provided.
  * Example: `./build.sh -DARCH=x86_64` or `ARCH=x86_64 ./build.sh`
  
  If you want to build in release mode pass in the RELEASE=1 environment variable.
  * Example: `RELEASE=1 ./build.sh -DARCH=x86_64` or `RELEASE=0 ./build.sh -DARCH=x86_64`

  If you want to disable pkg-config linking you can provide the `NO_PKGCFG` flag. 

  By default pkg-config linking is enabled. dep_root will be used when disabled.
  * Example: `./build.sh -DARCH=x86_64 -DNO_PKGCFG=1`
  * or `NO_PKGCFG=1 ./build.sh -DARCH=x86_64`
  
  If you want to overwrite the compiler on mac you can provide `NO_XCODE` flag.
  * Example: `CC=gcc CXX=g++ ./build.sh -DARCH=x86_64 -DNO_XCODE=1` 
  * or `NO_XCODE=1 CC=gcc CXX=g++ ./build.sh -DARCH=x86_64`

  If you want to disable cmake reconfigure for each build, you can provide the `NO_CLEAN` flag.
  * Example: `NO_CLEAN=1 ./build.sh -DARCH=x86_64`
  * By default it will remove cmake and cache and reconfigure for each subsequent build.
  
  If you enable the os built in AddressSanitizer feature use the `ASAN` flag.
  * Example: `ASAN=1 ./build.sh -DARCH=x86_64`
  * or `./build.sh -DARCH=x86_64 -DASAN=1`
  
  The compiled binary will be located at:
  * `cmake-build-release/src/futurerestore` for release builds
  * `cmake-build-debug/src/futurerestore` for debug builds
  
  Otherwise you can install the binary via:
  * `make -C cmake-build-release install` for release builds
  * `make -C cmake-build-debug install` for debug builds