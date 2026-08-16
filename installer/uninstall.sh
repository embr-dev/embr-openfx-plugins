#!/usr/bin/env bash
# EmbrSAM2 アンインストーラー
set -euo pipefail

PREFIX="${EMBR_SAM2_HOME:-$HOME/EmbrSAM2}"
OFX_DIR="${OFX_PLUGIN_DIR:-$HOME/OFX/Plugins}"

echo "EmbrSAM2 を削除します。"
echo "  ${OFX_DIR}/EmbrSAM2.ofx.bundle"
echo "  ${PREFIX}"
read -r -p "よろしいですか？ [y/N] " ans
case "${ans}" in
  y|Y|yes|YES) ;;
  *) echo "キャンセルしました。"; exit 0 ;;
esac

rm -rf "${OFX_DIR}/EmbrSAM2.ofx.bundle"
rm -rf "${PREFIX}"
echo "削除しました。ホストを再起動してください。"
