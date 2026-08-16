#!/usr/bin/env bash
# EmbrMatAnyone2 Linux installer
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

run_root mkdir -p "${OFX_DIR}" "${PREFIX}/models" "${PREFIX}/bin"
run_root rm -rf "${OFX_DIR}/EmbrMatAnyone2.ofx.bundle"
run_root cp -a "${SCRIPT_DIR}/EmbrMatAnyone2.ofx.bundle" "${OFX_DIR}/"

if [[ -d "${SCRIPT_DIR}/models" ]]; then
  run_root cp -a "${SCRIPT_DIR}/models/." "${PREFIX}/models/"
fi

TMP="$(mktemp)"
cat > "${TMP}" <<EOF
export EMBR_MATANYONE2_HOME="${PREFIX}"
export OFX_PLUGIN_PATH="\${OFX_PLUGIN_PATH:+\$OFX_PLUGIN_PATH:}${OFX_DIR}"
EOF
run_root cp -f "${TMP}" "${PREFIX}/env.sh"
rm -f "${TMP}"

[[ -f "${SCRIPT_DIR}/uninstall.sh" ]] && run_root cp -f "${SCRIPT_DIR}/uninstall.sh" "${PREFIX}/bin/uninstall.sh"
[[ -f "${SCRIPT_DIR}/使い方.txt" ]] && run_root cp -f "${SCRIPT_DIR}/使い方.txt" "${PREFIX}/使い方.txt"
run_root chmod +x "${PREFIX}/bin/uninstall.sh" 2>/dev/null || true

echo "完了。"
echo "ホスト再起動後: Embr → EmbrMatAnyone2"
echo "配線: Source + Mask(初フレーム/SAM2出力) → 本ノード"
echo "モデル(任意・次段の神経推論用): ${PREFIX}/models/matanyone2.pth"
