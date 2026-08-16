#!/usr/bin/env bash
# EmbrSAM2 Linux インストーラー
# ZIPを展開したディレクトリで実行してください: ./install.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PREFIX="${EMBR_SAM2_HOME:-$HOME/EmbrSAM2}"
OFX_DIR="${OFX_PLUGIN_DIR:-$HOME/OFX/Plugins}"

echo "========================================"
echo " EmbrSAM2 インストーラー (Linux)"
echo "========================================"
echo
echo "インストール先:"
echo "  アプリデータ : ${PREFIX}"
echo "  OFXプラグイン: ${OFX_DIR}/EmbrSAM2.ofx.bundle"
echo

if [[ ! -d "${SCRIPT_DIR}/EmbrSAM2.ofx.bundle" ]]; then
  echo "エラー: EmbrSAM2.ofx.bundle が見つかりません。"
  echo "ZIPを展開したフォルダで ./install.sh を実行してください。"
  exit 1
fi

mkdir -p "${PREFIX}/models/sam2"
mkdir -p "${PREFIX}/bin"
mkdir -p "${OFX_DIR}"

# 既存バンドルを置き換え
rm -rf "${OFX_DIR}/EmbrSAM2.ofx.bundle"
cp -a "${SCRIPT_DIR}/EmbrSAM2.ofx.bundle" "${OFX_DIR}/"

# モデル（ZIPに含まれていればコピー。無ければプレースホルダのみ）
if [[ -d "${SCRIPT_DIR}/models/sam2" ]]; then
  cp -a "${SCRIPT_DIR}/models/sam2/." "${PREFIX}/models/sam2/"
fi

# 環境ファイル（ホスト起動前に source してもよい）
cat > "${PREFIX}/env.sh" <<EOF
# EmbrSAM2 environment
export EMBR_SAM2_HOME="${PREFIX}"
# OFX 標準検索パス（多くのホストが参照）
export OFX_PLUGIN_PATH="\${OFX_PLUGIN_PATH:-}:${OFX_DIR}"
EOF

cp -f "${SCRIPT_DIR}/uninstall.sh" "${PREFIX}/bin/uninstall.sh" 2>/dev/null || true
chmod +x "${PREFIX}/bin/uninstall.sh" 2>/dev/null || true

HAS_ENC=0
HAS_DEC=0
[[ -f "${PREFIX}/models/sam2/image_encoder.onnx" ]] && HAS_ENC=1
[[ -f "${PREFIX}/models/sam2/image_decoder.onnx" ]] && HAS_DEC=1

echo "完了しました。"
echo
echo "【次にやること】"
echo "1. ホスト（Flame / Resolve など）を再起動する"
echo "2. エフェクト一覧の Embr → EmbrSAM2 を選ぶ"
echo
if [[ "${HAS_ENC}" -ne 1 || "${HAS_DEC}" -ne 1 ]]; then
  echo "【モデルについて】"
  echo "まだ ONNX モデルがありません。今は demo mode（楕円マスク）で動作します。"
  echo "本番推論するには次の2ファイルを置いてください:"
  echo "  ${PREFIX}/models/sam2/image_encoder.onnx"
  echo "  ${PREFIX}/models/sam2/image_decoder.onnx"
  echo
else
  echo "モデルファイルを検出したので、デフォルトパスで推論できます。"
  echo
fi
echo "詳細は同じフォルダの「使い方.txt」を読んでください。"
