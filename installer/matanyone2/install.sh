#!/usr/bin/env bash
# EmbrMatAnyone2 Linux installer (OFX + neural runtime)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OFX_DIR="${OFX_PLUGIN_DIR:-/usr/OFX/Plugins}"
PREFIX="${EMBR_MATANYONE2_HOME:-/opt/Embr/EmbrMatAnyone2}"

run_root() {
  if [[ "$(id -u)" -eq 0 ]]; then "$@"
  elif command -v sudo >/dev/null 2>&1; then sudo "$@"
  else echo "root/sudo required"; exit 1
  fi
}

echo "========================================"
echo " EmbrMatAnyone2 インストーラー (Linux)"
echo "========================================"
echo "  OFX : ${OFX_DIR}/EmbrMatAnyone2.ofx.bundle"
echo "  Data: ${PREFIX}"
echo

if [[ ! -d "${SCRIPT_DIR}/EmbrMatAnyone2.ofx.bundle" ]]; then
  echo "エラー: EmbrMatAnyone2.ofx.bundle がありません"
  exit 1
fi

run_root mkdir -p "${OFX_DIR}" \
  "${PREFIX}/models" \
  "${PREFIX}/bin" \
  "${PREFIX}/python" \
  "${PREFIX}/src"

run_root rm -rf "${OFX_DIR}/EmbrMatAnyone2.ofx.bundle"
run_root cp -a "${SCRIPT_DIR}/EmbrMatAnyone2.ofx.bundle" "${OFX_DIR}/"

if [[ -d "${SCRIPT_DIR}/models" ]]; then
  run_root cp -a "${SCRIPT_DIR}/models/." "${PREFIX}/models/"
fi

if [[ -d "${SCRIPT_DIR}/python" ]]; then
  run_root cp -a "${SCRIPT_DIR}/python/." "${PREFIX}/python/"
fi

if [[ -d "${SCRIPT_DIR}/src/MatAnyone2" ]]; then
  run_root rm -rf "${PREFIX}/src/MatAnyone2"
  run_root cp -a "${SCRIPT_DIR}/src/MatAnyone2" "${PREFIX}/src/"
fi

# Create / refresh venv for neural inference
PY_SYS="${EMBR_PYTHON:-python3}"
if ! command -v "${PY_SYS}" >/dev/null 2>&1; then
  echo "警告: python3 が無く、神経推論用 venv を作れません（demo のみ）"
else
  echo "神経推論用 venv を準備します..."
  run_root "${PY_SYS}" -m venv "${PREFIX}/venv" || {
    echo "警告: python3-venv が必要です (apt install python3-venv)"
  }
  if [[ -x "${PREFIX}/venv/bin/pip" ]]; then
    # Prefer CUDA wheel when requested; default CPU for broad installers.
    TORCH_INDEX="${EMBR_TORCH_INDEX:-https://download.pytorch.org/whl/cpu}"
    run_root "${PREFIX}/venv/bin/pip" install -U pip
    run_root "${PREFIX}/venv/bin/pip" install \
      torch torchvision --index-url "${TORCH_INDEX}"
    run_root "${PREFIX}/venv/bin/pip" install \
      'numpy>=1.21' 'Pillow>=9.5' 'opencv-python-headless>=4.8' \
      'omegaconf>=2.3' 'hydra-core>=1.3.2' einops tqdm \
      huggingface_hub safetensors scipy 'imageio==2.25.0' imageio-ffmpeg
    echo "venv OK: ${PREFIX}/venv"
  fi
fi

TMP="$(mktemp)"
cat > "${TMP}" <<EOF
export EMBR_MATANYONE2_HOME="${PREFIX}"
export OFX_PLUGIN_PATH="\${OFX_PLUGIN_PATH:+\$OFX_PLUGIN_PATH:}${OFX_DIR}"
export PYTHONPATH="\${EMBR_MATANYONE2_HOME}/src/MatAnyone2:\${PYTHONPATH:-}"
EOF
run_root cp -f "${TMP}" "${PREFIX}/env.sh"
rm -f "${TMP}"

[[ -f "${SCRIPT_DIR}/uninstall.sh" ]] && run_root cp -f "${SCRIPT_DIR}/uninstall.sh" "${PREFIX}/bin/uninstall.sh"
[[ -f "${SCRIPT_DIR}/使い方.txt" ]] && run_root cp -f "${SCRIPT_DIR}/使い方.txt" "${PREFIX}/使い方.txt"
run_root chmod +x "${PREFIX}/bin/uninstall.sh" 2>/dev/null || true
run_root chmod +x "${PREFIX}/python/"*.py 2>/dev/null || true

echo "完了。"
echo "ホスト再起動後: Embr → EmbrMatAnyone2"
echo "配線: Source + Mask(初フレーム/SAM2出力) → 本ノード"
if [[ -f "${PREFIX}/models/matanyone2.pth" && -x "${PREFIX}/venv/bin/python" && -d "${PREFIX}/src/MatAnyone2/matanyone2" ]]; then
  echo "本推論: 有効 (Prefer Neural=ON)"
else
  echo "本推論: 未完了 → demo soft-matte（models / venv / src を確認）"
fi
echo "モデル: ${PREFIX}/models/matanyone2.pth"
echo "Python: ${PREFIX}/venv/bin/python"
