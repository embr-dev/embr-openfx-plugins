#!/usr/bin/env bash
# EmbrSAM2 アンインストーラー
set -euo pipefail

OFX_DIR="${OFX_PLUGIN_DIR:-/usr/OFX/Plugins}"
PREFIX="${EMBR_SAM2_HOME:-/opt/Embr/EmbrSAM2}"

run_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    echo "エラー: root 権限が必要です。"
    exit 1
  fi
}

echo "EmbrSAM2 を削除します。"
echo "  ${OFX_DIR}/EmbrSAM2.ofx.bundle"
echo "  ${PREFIX}"
read -r -p "よろしいですか？ [y/N] " ans
case "${ans}" in
  y|Y|yes|YES) ;;
  *) echo "キャンセルしました。"; exit 0 ;;
esac

run_root rm -rf "${OFX_DIR}/EmbrSAM2.ofx.bundle"
run_root rm -rf "${PREFIX}"

# 旧パスの掃除（任意）
if [[ -d "${HOME}/OFX/Plugins/EmbrSAM2.ofx.bundle" ]]; then
  rm -rf "${HOME}/OFX/Plugins/EmbrSAM2.ofx.bundle"
  echo "旧パス ${HOME}/OFX/Plugins/EmbrSAM2.ofx.bundle も削除しました。"
fi

echo "削除しました。ホストを再起動してください。"
