#!/usr/bin/env zsh

set -e
export BASE=${TMPDIR}/Builder/repos/futurerestore/.github/workflows

cd ${BASE}/../../
export FUTURERESTORE_VERSION=$(git rev-list --count HEAD | tr -d '\n')
export FUTURERESTORE_VERSION_RELEASE=$(cat version.txt | tr -d '\n')
cd ${BASE}
cp -RpP "${TMPDIR}/Builder/macOS_x86_64_1300_Release/bin/futurerestore" futurerestore
tar cpPJf "futurerestore-macOS-x86_64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-RELEASE.tar.xz" futurerestore
cp -RpP "${TMPDIR}/Builder/macOS_x86_64_1300_Debug/bin/futurerestore" futurerestore
tar cpPJf "futurerestore-macOS-x86_64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-DEBUG.tar.xz" futurerestore
cp -RpP "${TMPDIR}/Builder/macOS_arm64_1700_Release/bin/futurerestore" futurerestore
tar cpPJf "futurerestore-macOS-arm64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-RELEASE.tar.xz" futurerestore
cp -RpP "${TMPDIR}/Builder/macOS_arm64_1700_Debug/bin/futurerestore" futurerestore
tar cpPJf "futurerestore-macOS-arm64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-DEBUG.tar.xz" futurerestore
