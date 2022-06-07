#!/usr/bin/env zsh

set -e
export WORKFLOW_ROOT=/Users/runner/work/futurerestore/futurerestore/.github/workflows
export DEP_ROOT=/Users/runner/work/futurerestore/futurerestore/dep_root
export BASE=/Users/runner/work/futurerestore/futurerestore/

cd ${BASE}
export FUTURERESTORE_VERSION=$(git rev-list --count HEAD | tr -d '\n')
export FUTURERESTORE_VERSION_RELEASE=$(cat version.txt | tr -d '\n')
tar cpPJf "futurerestore-macOS-x86_64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-RELEASE.tar.xz" -C cmake-build-release-x86_64/src futurerestore
tar cpPJf "futurerestore-macOS-x86_64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-DEBUG.tar.xz" -C cmake-build-debug-x86_64/src futurerestore
tar cpPJf "futurerestore-macOS-arm64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-RELEASE.tar.xz" -C cmake-build-release-arm64/src futurerestore
tar cpPJf "futurerestore-macOS-arm64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-DEBUG.tar.xz" -C cmake-build-debug-arm64/src futurerestore
