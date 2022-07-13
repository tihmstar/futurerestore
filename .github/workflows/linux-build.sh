#!/usr/bin/env bash

set -e
export TMPDIR=/tmp
export WORKFLOW_ROOT=${TMPDIR}/Builder/repos/futurerestore/.github/workflows
export DEP_ROOT=${TMPDIR}/Builder/repos/futurerestore/dep_root
export BASE=${TMPDIR}/Builder/repos/futurerestore/

cd ${BASE}
ln -sf ${DEP_ROOT}/Linux_x86_64_Release/{lib/,include/}  ${DEP_ROOT}/
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -DCMAKE_CXX_COMPILER=$(which clang++) -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-release-x86_64 -DARCH=x86_64 -DCMAKE_C_COMPILER=clang-13 -DCMAKE_CXX_COMPILER=clang++-13 -DCMAKE_LINKER=ld.lld-13 -DNO_PKGCFG=1
make -j4 -l4 -C cmake-build-release-x86_64

cd ${BASE}
ln -sf ${DEP_ROOT}/Linux_x86_64_Debug/{lib/,include/} ${DEP_ROOT}/
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-debug-x86_64 -DARCH=x86_64 -DCMAKE_C_COMPILER=clang-13 -DCMAKE_CXX_COMPILER=clang++-13 -DCMAKE_LINKER=ld.lld-13 -DNO_PKGCFG=1
make -j4 -l4 -C cmake-build-debug-x86_64

cd ${BASE}
ln -sf ${DEP_ROOT}/Linux_x86_64_Debug/{lib/,include/} ${DEP_ROOT}/
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=$(which make) -DCMAKE_C_COMPILER=$(which clang) -DCMAKE_CXX_COMPILER=$(which clang++) -DCMAKE_MESSAGE_LOG_LEVEL="WARNING" -G "CodeBlocks - Unix Makefiles" -S ./ -B cmake-build-asan-x86_64 -DARCH=x86_64 -DCMAKE_C_COMPILER=clang-13 -DCMAKE_CXX_COMPILER=clang++-13 -DCMAKE_LINKER=ld.lld-13 -DNO_PKGCFG=1 -DASAN=1
make -j4 -l4 -C cmake-build-asan-x86_64

llvm-strip-13 -s cmake-build-release-x86_64/src/futurerestore
