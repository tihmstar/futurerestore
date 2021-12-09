#!/usr/bin/env bash

set -e
export TMPDIR=/tmp
export BASE=${TMPDIR}/Builder/repos/futurerestore/.github/workflows

cd ${BASE}/../../
export FUTURERESTORE_VERSION=$(git rev-list --count HEAD | tr -d '\n')
export FUTURERESTORE_VERSION_RELEASE=$(cat version.txt | tr -d '\n')
cd ${BASE}
echo "futurerestore-Linux-x86_64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-RELEASE.tar.xz" > name1.txt
echo "futurerestore-Linux-x86_64-${FUTURERESTORE_VERSION_RELEASE}-Build_${FUTURERESTORE_VERSION}-DEBUG.tar.xz" > name2.txt
cp -RpP "${TMPDIR}/Builder/Linux_x86_64_Release/bin/futurerestore" futurerestore
tar cpPJvf "futurerestore1.tar.xz" futurerestore
cp -RpP "${TMPDIR}/Builder/Linux_x86_64_Debug/bin/futurerestore" futurerestore
tar cpPJvf "futurerestore2.tar.xz" futurerestore
