#!/usr/bin/env zsh

set -e
export WORKFLOW_ROOT=/Users/runner/work/futurerestore/futurerestore/.github/workflows
export DEP_ROOT=/Users/runner/work/futurerestore/futurerestore/dep_root
export BASE=/Users/runner/work/futurerestore/futurerestore/

cd ${BASE}
export FUTURERESTORE_VERSION=$(git rev-list --count HEAD | tr -d '\n')
export FUTURERESTORE_VERSION_RELEASE=$(cat version.txt | tr -d '\n')
lipo -create -arch x86_64 cmake-build-release-x86_64/src/futurerestore -arch arm64 cmake-build-release-arm64/src/futurerestore -output futurerestore
tar cpPJf "futurerestore-macOS-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-RELEASE.tar.xz" futurerestore
rm -rf futurerestore
lipo -create -arch x86_64 cmake-build-debug-x86_64/src/futurerestore -arch arm64 cmake-build-debug-arm64/src/futurerestore -output futurerestore
tar cpPJf "futurerestore-macOS-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-DEBUG.tar.xz" futurerestore
rm -rf futurerestore
lipo -create -arch x86_64 cmake-build-asan-x86_64/src/futurerestore -arch arm64 cmake-build-asan-arm64/src/futurerestore -output futurerestore
tar cpPJf "futurerestore-macOS-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-ASAN.tar.xz" futurerestore
