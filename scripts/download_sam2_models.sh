#!/usr/bin/env bash
# Download SAM2 ONNX encoder/decoder into models/sam2/
# Default: tiny (smaller installer). Override with SAM2_MODEL_SIZE=small|base_plus|large
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${ROOT}/models/sam2"
SIZE="${SAM2_MODEL_SIZE:-tiny}"
REPO="vietanhdev/segment-anything-2-onnx-models"

case "${SIZE}" in
  tiny)      BASE="sam2_hiera_tiny" ;;
  small)     BASE="sam2_hiera_small" ;;
  base_plus) BASE="sam2_hiera_base_plus" ;;
  large)     BASE="sam2_hiera_large" ;;
  *)
    echo "Unknown SAM2_MODEL_SIZE=${SIZE} (use tiny|small|base_plus|large)"
    exit 2
    ;;
esac

ENC_NAME="${BASE}.encoder.onnx"
DEC_NAME="${BASE}.decoder.onnx"
mkdir -p "${OUT}"

echo "Downloading ${ENC_NAME} / ${DEC_NAME} from ${REPO} ..."

download() {
  local file="$1"
  local dest="$2"
  local url="https://huggingface.co/${REPO}/resolve/main/${file}"
  echo "  -> ${url}"
  curl -fL --retry 3 --retry-delay 2 "${url}" -o "${dest}.partial"
  mv "${dest}.partial" "${dest}"
}

download "${ENC_NAME}" "${OUT}/image_encoder.onnx"
download "${DEC_NAME}" "${OUT}/image_decoder.onnx"

# Provenance note for redistributors
cat > "${OUT}/SOURCE.txt" <<EOF
SAM2 ONNX models for EmbrSAM2
------------------------------
Source repo : https://huggingface.co/${REPO}
Files       : ${ENC_NAME}, ${DEC_NAME}
Saved as    : image_encoder.onnx, image_decoder.onnx
Size preset : ${SIZE}
Upstream    : Meta SAM 2 (Apache-2.0) — https://github.com/facebookresearch/sam2
EOF

ls -lh "${OUT}/image_encoder.onnx" "${OUT}/image_decoder.onnx"
echo "Done. Models are in ${OUT}"
