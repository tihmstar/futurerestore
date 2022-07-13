#!/usr/bin/env zsh

set -e
export WORKFLOW_ROOT=/Users/runner/work/futurerestore/futurerestore/.github/workflows
export DEP_ROOT=/Users/runner/work/futurerestore/futurerestore/dep_root
export BASE=/Users/runner/work/futurerestore/futurerestore/

cd /Users/runner/work/futurerestore/futurerestore/
ln -sf ${DEP_ROOT}/macOS_x86_64_Release/{lib/,include/} ${DEP_ROOT}/
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-release-x86_64 -DARCH=x86_64 -DNO_PKGCFG=1
make -j4 -l4 -C cmake-build-release-x86_64

ln -sf ${DEP_ROOT}/macOS_x86_64_Debug/{lib/,include/} ${DEP_ROOT}/
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-debug-x86_64 -DARCH=x86_64 -DNO_PKGCFG=1
make -j4 -l4 -C cmake-build-debug-x86_64

ln -sf ${DEP_ROOT}/macOS_arm64_Release/{lib/,include/} ${DEP_ROOT}/
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-release-arm64 -DARCH=arm64 -DNO_PKGCFG=1
make -j4 -l4 -C cmake-build-release-arm64

ln -sf ${DEP_ROOT}/macOS_arm64_Debug/{lib/,include/} ${DEP_ROOT}/
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-debug-arm64 -DARCH=arm64 -DNO_PKGCFG=1
make -j4 -l4 -C cmake-build-debug-arm64

ln -sf ${DEP_ROOT}/macOS_x86_64_Debug/{lib/,include/} ${DEP_ROOT}/
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-asan-x86_64 -DARCH=x86_64 -DNO_PKGCFG=1 -DASAN=1
make -j4 -l4 -C cmake-build-asan-x86_64

ln -sf ${DEP_ROOT}/macOS_arm64_Debug/{lib/,include/} ${DEP_ROOT}/
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-asan-arm64 -DARCH=arm64 -DNO_PKGCFG=1 -DASAN=1
make -j4 -l4 -C cmake-build-asan-arm64


llvm-strip -s cmake-build-release-x86_64/src/futurerestore
llvm-strip -s cmake-build-release-arm64/src/futurerestore
