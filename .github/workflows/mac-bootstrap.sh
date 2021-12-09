#!/usr/bin/env zsh

set -e
export BASE=/Users/runner/work/futurerestore/futurerestore/.github/workflows

cd ${BASE}
curl -sO https://cdn.cryptiiiic.com/bootstrap/bootstrap_x86_64.tar.zst &
curl -sO https://cdn.cryptiiiic.com/bootstrap/Builder_macOS.tar.zst &
curl -sO https://cdn.cryptiiiic.com/deps/static/macOS/x86_64/macOS_x86_64_1300_Release_Latest.tar.zst &
curl -sO https://cdn.cryptiiiic.com/deps/static/macOS/arm64/macOS_arm64_1700_Release_Latest.tar.zst &
curl -sO https://cdn.cryptiiiic.com/deps/static/macOS/x86_64/macOS_x86_64_1300_Debug_Latest.tar.zst &
curl -sO https://cdn.cryptiiiic.com/deps/static/macOS/arm64/macOS_arm64_1700_Debug_Latest.tar.zst &
wait
sudo gtar xf ${BASE}/bootstrap_x86_64.tar.zst -C / --warning=none || true || true &
echo "${PROCURSUS}/bin" | sudo tee /etc/paths1
echo "${PROCURSUS}/libexec/gnubin" | sudo tee /etc/paths1
cat /etc/paths | sudo tee -a /etc/paths1
sudo mv /etc/paths{1,}
wait
mkdir -p ${TMPDIR}/Builder/repos
gtar xf macOS_x86_64_1300_Release_Latest.tar.zst -C ${TMPDIR}/Builder &
gtar xf macOS_x86_64_1300_Debug_Latest.tar.zst -C ${TMPDIR}/Builder &
gtar xf macOS_arm64_1700_Release_Latest.tar.zst -C ${TMPDIR}/Builder &
gtar xf macOS_arm64_1700_Debug_Latest.tar.zst -C ${TMPDIR}/Builder &
wait
gtar xf ${BASE}/Builder_macOS.tar.zst &
sudo ${PROCURSUS}/bin/apt-get update -qq
sudo ${PROCURSUS}/bin/apt-get dist-upgrade -yqq
sudo mv /usr/local/bin{,1}
ln -sf ${BASE}/../../ ${TMPDIR}/Builder/repos/futurerestore
cd ${TMPDIR}/Builder/repos/futurerestore
git submodule update --init --recursive
cd ${TMPDIR}/Builder/repos/futurerestore/.github/workflows
