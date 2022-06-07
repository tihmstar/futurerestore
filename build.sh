#!/bin/bash
if [[ -z "$NO_CLEAN" ]]; then rm -rf cmake-build-release cmake-build-debug; fi
if [[ "$RELEASE" == "1" ]]
then
  if [[ ! "$NO_CLEAN" == "1" ]]; then cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-release $@; fi
  make -C cmake-build-release clean
  make -C cmake-build-release
  llvm-strip -s cmake-build-release/src/futurerestore
else
  if [[ ! "$NO_CLEAN" == "1" ]]; then cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-debug $@ ; fi
  make -C cmake-build-debug clean
  make -C cmake-build-debug
fi
