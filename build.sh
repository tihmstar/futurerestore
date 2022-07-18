#!/usr/bin/env bash
which cmake >/dev/null
 if [[ "$?" -gt 0 ]]; then echo "[!] cmake not installed or not found, refusing to build!"; exit 1; fi
which make >/dev/null
if [[ "$?" -gt 0 ]]; then echo "[!] make not installed or not found, refusing to build!"; exit 1; fi
export CC="$(which clang)"
export CXX="$(which clang++)"
which xcrun >/dev/null
 if [[ "$?" -lt 1 ]]; then export CC="$(xcrun --find clang)"; export CXX="$(xcrun --find clang++)"; fi
echo "[*] Building futurerestore"
export FR_INSTALL_DIR="/usr/local"
if [[ -z "$NO_CLEAN" ]]; then rm -rf cmake-build-release cmake-build-debug; fi
if [[ "$RELEASE" == "1" ]]
then
  if [[ ! "$NO_CLEAN" == "1" ]]; then cmake -DCMAKE_INSTALL_PREFIX="${FR_INSTALL_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER="${CC}" -DCMAKE_CXX_COMPILER="${CXX}" -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-release $@; fi
  make -s -C cmake-build-release clean
  make -s -C cmake-build-release
  if [[ "$?" -gt 0 ]]; then echo "[!] Failed to build futurerestore!"; exit 1; fi
  which llvm-strip >/dev/null
  if [[ "$?" -lt 1 ]]; then llvm-strip -s cmake-build-release/src/futurerestore; else echo "[!] llvm-strip not installed or not found, not stripping release binary."; fi
  echo "[*] Run make -C cmake-build-release install, to install futurerestore or obtain the binary at cmake-build-release/src/futurerestore"
  echo "[*] Successfully built futurerestore."
else
  if [[ ! "$NO_CLEAN" == "1" ]]; then cmake -DCMAKE_INSTALL_PREFIX="${FR_INSTALL_DIR}" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER="${CC}" -DCMAKE_CXX_COMPILER="${CXX}"  -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-debug $@ ; fi
  make -s -C cmake-build-debug clean
  make -s -C cmake-build-debug
  if [[ "$?" -gt 0 ]]; then echo "[!] Failed to build futurerestore!"; exit 1; fi
  echo "[*] Run make -C cmake-build-debug install, to install futurerestore or obtain the binary at cmake-build-debug/src/futurerestore"
  echo "[*] Successfully built futurerestore."
fi
