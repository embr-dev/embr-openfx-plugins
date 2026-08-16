#!/usr/bin/env bash
# ビルド成果物から Linux インストーラー ZIP を作成する
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
DIST="${DIST_DIR:-$ROOT/dist}"
VER="${EMBR_SAM2_VERSION:-0.1.0}"
STAGE="${DIST}/EmbrSAM2-linux-x86_64-${VER}"
ZIP="${DIST}/EmbrSAM2-linux-x86_64-${VER}.zip"

BUNDLE_SRC="${BUILD}/ofx/EmbrSAM2.ofx.bundle"
OFX_BIN="${BUNDLE_SRC}/Contents/Linux-x86-64/EmbrSAM2.ofx"

if [[ ! -f "${OFX_BIN}" ]]; then
  echo "エラー: 先にビルドしてください:"
  echo "  ./scripts/download_onnxruntime_linux.sh"
  echo "  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
  exit 1
fi

# バンドルに ORT が無い場合は補完
if [[ ! -e "${BUNDLE_SRC}/Contents/Linux-x86-64/libonnxruntime.so" ]]; then
  echo "ONNX Runtime をバンドルへコピーします..."
  cp -a "${ROOT}/third_party/onnxruntime/lib"/libonnxruntime.so* \
        "${BUNDLE_SRC}/Contents/Linux-x86-64/" || true
  cp -a "${ROOT}/third_party/onnxruntime/lib"/libonnxruntime_providers_*.so \
        "${BUNDLE_SRC}/Contents/Linux-x86-64/" 2>/dev/null || true
fi

rm -rf "${STAGE}"
mkdir -p "${STAGE}"

cp -a "${BUNDLE_SRC}" "${STAGE}/"
cp -a "${ROOT}/installer/install.sh" "${STAGE}/"
cp -a "${ROOT}/installer/uninstall.sh" "${STAGE}/"
cp -a "${ROOT}/installer/使い方.txt" "${STAGE}/"
mkdir -p "${STAGE}/models/sam2"
cp -a "${ROOT}/installer/models/sam2/." "${STAGE}/models/sam2/"

# リポジトリにモデルがあれば同梱
if [[ -f "${ROOT}/models/sam2/image_encoder.onnx" ]]; then
  cp -f "${ROOT}/models/sam2/image_encoder.onnx" "${STAGE}/models/sam2/"
fi
if [[ -f "${ROOT}/models/sam2/image_decoder.onnx" ]]; then
  cp -f "${ROOT}/models/sam2/image_decoder.onnx" "${STAGE}/models/sam2/"
fi

chmod +x "${STAGE}/install.sh" "${STAGE}/uninstall.sh"

# LICENSE
cp -f "${ROOT}/LICENSE" "${STAGE}/LICENSE.txt" 2>/dev/null || true

rm -f "${ZIP}"
mkdir -p "${DIST}"
(
  cd "${DIST}"
  zip -r -q "$(basename "${ZIP}")" "$(basename "${STAGE}")"
)

echo "作成しました: ${ZIP}"
echo
echo "利用者向け手順:"
echo "  unzip $(basename "${ZIP}")"
echo "  cd $(basename "${STAGE}")"
echo "  ./install.sh"
ls -lh "${ZIP}"
