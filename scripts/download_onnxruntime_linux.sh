#!/usr/bin/env bash
# Download official ONNX Runtime Linux x64 CPU package into third_party/onnxruntime
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VER="${ONNXRUNTIME_VERSION:-1.19.2}"
URL="https://github.com/microsoft/onnxruntime/releases/download/v${VER}/onnxruntime-linux-x64-${VER}.tgz"
DEST="${ROOT}/third_party/onnxruntime"
TMP="$(mktemp -d)"

echo "Downloading ${URL}"
curl -fL "${URL}" -o "${TMP}/ort.tgz"
rm -rf "${DEST}"
mkdir -p "${ROOT}/third_party"
tar -xzf "${TMP}/ort.tgz" -C "${TMP}"
mv "${TMP}/onnxruntime-linux-x64-${VER}" "${DEST}"
rm -rf "${TMP}"
echo "Installed ONNX Runtime to ${DEST}"
ls "${DEST}/lib"
