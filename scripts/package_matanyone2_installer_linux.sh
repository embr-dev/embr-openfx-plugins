#!/usr/bin/env bash
# Package EmbrMatAnyone2 Linux installer ZIP
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
DIST="${DIST_DIR:-$ROOT/dist}"
VER="${EMBR_MATANYONE2_VERSION:-0.1.0}"
STAGE="${DIST}/EmbrMatAnyone2-linux-x86_64-${VER}"
ZIP="${DIST}/EmbrMatAnyone2-linux-x86_64-${VER}.zip"
BUNDLE="${BUILD}/ofx/EmbrMatAnyone2.ofx.bundle"

if [[ ! -f "${BUNDLE}/Contents/Linux-x86-64/EmbrMatAnyone2.ofx" ]]; then
  echo "Build first: cmake --build build --target EmbrMatAnyone2"
  exit 1
fi

rm -rf "${STAGE}"
mkdir -p "${STAGE}/models"
cp -a "${BUNDLE}" "${STAGE}/"
cp -a "${ROOT}/installer/matanyone2/install.sh" "${STAGE}/"
cp -a "${ROOT}/installer/matanyone2/uninstall.sh" "${STAGE}/"
cp -a "${ROOT}/installer/matanyone2/使い方.txt" "${STAGE}/"
cp -f "${ROOT}/LICENSE" "${STAGE}/LICENSE.txt" 2>/dev/null || true

if [[ -f "${ROOT}/models/matanyone2/matanyone2.pth" ]]; then
  cp -f "${ROOT}/models/matanyone2/matanyone2.pth" "${STAGE}/models/"
  cp -f "${ROOT}/models/matanyone2/SOURCE.txt" "${STAGE}/models/" 2>/dev/null || true
else
  echo "注: matanyone2.pth 未同梱（./scripts/download_matanyone2_weights.sh）"
  echo "OFX demo soft-matte は重み無しでも動作します。" > "${STAGE}/models/README.txt"
fi

chmod +x "${STAGE}/install.sh" "${STAGE}/uninstall.sh"
mkdir -p "${DIST}"
rm -f "${ZIP}"
( cd "${DIST}" && zip -r -q "$(basename "${ZIP}")" "$(basename "${STAGE}")" )
echo "作成: ${ZIP}"
ls -lh "${ZIP}"
