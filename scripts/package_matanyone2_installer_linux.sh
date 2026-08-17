#!/usr/bin/env bash
# Package EmbrMatAnyone2 Linux installer ZIP (OFX + neural runtime assets)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
DIST="${DIST_DIR:-$ROOT/dist}"
VER="${EMBR_MATANYONE2_VERSION:-0.1.1}"
STAGE="${DIST}/EmbrMatAnyone2-linux-x86_64-${VER}"
ZIP="${DIST}/EmbrMatAnyone2-linux-x86_64-${VER}.zip"
BUNDLE="${BUILD}/ofx/EmbrMatAnyone2.ofx.bundle"
MAT_SRC="${MATANYONE2_SRC:-$ROOT/third_party/MatAnyone2}"

if [[ ! -f "${BUNDLE}/Contents/Linux-x86-64/EmbrMatAnyone2.ofx" ]]; then
  echo "Build first: cmake --build build --target EmbrMatAnyone2"
  exit 1
fi

if [[ ! -d "${MAT_SRC}/matanyone2" ]]; then
  echo "MatAnyone2 source missing. Clone:"
  echo "  git clone --depth 1 https://github.com/pq-yang/MatAnyone2.git third_party/MatAnyone2"
  exit 1
fi

rm -rf "${STAGE}"
mkdir -p "${STAGE}/models" "${STAGE}/python" "${STAGE}/src"

cp -a "${BUNDLE}" "${STAGE}/"
cp -a "${ROOT}/installer/matanyone2/install.sh" "${STAGE}/"
cp -a "${ROOT}/installer/matanyone2/uninstall.sh" "${STAGE}/"
cp -a "${ROOT}/installer/matanyone2/使い方.txt" "${STAGE}/"
cp -f "${ROOT}/LICENSE" "${STAGE}/LICENSE.txt" 2>/dev/null || true
cp -a "${ROOT}/plugins/matanyone2/python/." "${STAGE}/python/"

# Vendor MatAnyone2 Python package (no .git)
rsync -a --delete \
  --exclude '.git' \
  --exclude '__pycache__' \
  --exclude '*.pyc' \
  --exclude '.venv' \
  --exclude 'inputs' \
  --exclude 'assets' \
  --exclude 'evaluation' \
  --exclude 'hugging_face' \
  --exclude 'docs' \
  "${MAT_SRC}/" "${STAGE}/src/MatAnyone2/"

if [[ -f "${ROOT}/models/matanyone2/matanyone2.pth" ]]; then
  cp -f "${ROOT}/models/matanyone2/matanyone2.pth" "${STAGE}/models/"
  cp -f "${ROOT}/models/matanyone2/SOURCE.txt" "${STAGE}/models/" 2>/dev/null || true
else
  echo "注: matanyone2.pth 未同梱（./scripts/download_matanyone2_weights.sh）"
  echo "install.sh 後に models/ へ配置してください。" > "${STAGE}/models/README.txt"
fi

chmod +x "${STAGE}/install.sh" "${STAGE}/uninstall.sh" "${STAGE}/python/"*.py
mkdir -p "${DIST}"
rm -f "${ZIP}"
( cd "${DIST}" && zip -r -q "$(basename "${ZIP}")" "$(basename "${STAGE}")" )
echo "作成: ${ZIP}"
ls -lh "${ZIP}"
