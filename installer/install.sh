#!/usr/bin/env bash
# EmbrSAM2 Linux インストーラー
# ZIPを展開したディレクトリで実行: ./install.sh
# 管理者権限が必要です（/usr/OFX/Plugins と /opt/Embr へ配置）
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 公式OFX標準パス（Flame / Resolve / Natron が参照）
# 参考: https://openfx.readthedocs.io/en/main/Reference/ofxPackaging.html
# Flame は /usr/OFX/Plugins に1つ以上ないと OpenFX ノードが出ないことがある
OFX_DIR="${OFX_PLUGIN_DIR:-/usr/OFX/Plugins}"

# モデル・設定の置き場（ユーザー指定イメージ: /opt/Embr/EmbrSAM2）
PREFIX="${EMBR_SAM2_HOME:-/opt/Embr/EmbrSAM2}"

run_root() {
  if [[ "$(id -u)" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    echo "エラー: root 権限が必要です（sudo を使うか root で実行してください）。"
    exit 1
  fi
}

echo "========================================"
echo " EmbrSAM2 インストーラー (Linux)"
echo "========================================"
echo
echo "インストール先:"
echo "  OFXプラグイン : ${OFX_DIR}/EmbrSAM2.ofx.bundle"
echo "  アプリデータ   : ${PREFIX}"
echo "                 （models / env.sh など）"
echo

if [[ ! -d "${SCRIPT_DIR}/EmbrSAM2.ofx.bundle" ]]; then
  echo "エラー: EmbrSAM2.ofx.bundle が見つかりません。"
  echo "ZIPを展開したフォルダで ./install.sh を実行してください。"
  exit 1
fi

# OFX バンドル構造チェック
if [[ ! -f "${SCRIPT_DIR}/EmbrSAM2.ofx.bundle/Contents/Linux-x86-64/EmbrSAM2.ofx" ]]; then
  echo "エラー: バンドル内に EmbrSAM2.ofx がありません。ZIPが壊れている可能性があります。"
  exit 1
fi

echo "ディレクトリを作成しています..."
run_root mkdir -p "${OFX_DIR}"
run_root mkdir -p "${PREFIX}/models/sam2"
run_root mkdir -p "${PREFIX}/bin"

echo "OFX プラグインを配置しています..."
run_root rm -rf "${OFX_DIR}/EmbrSAM2.ofx.bundle"
run_root cp -a "${SCRIPT_DIR}/EmbrSAM2.ofx.bundle" "${OFX_DIR}/"

echo "モデル・データを配置しています..."
if [[ -d "${SCRIPT_DIR}/models/sam2" ]]; then
  run_root cp -a "${SCRIPT_DIR}/models/sam2/." "${PREFIX}/models/sam2/"
fi

# 環境ファイル
TMP_ENV="$(mktemp)"
cat > "${TMP_ENV}" <<EOF
# EmbrSAM2 environment
export EMBR_SAM2_HOME="${PREFIX}"
# 標準パスに加えて明示（通常は不要だが Flame 等で上書きされている場合用）
export OFX_PLUGIN_PATH="\${OFX_PLUGIN_PATH:+\$OFX_PLUGIN_PATH:}${OFX_DIR}"
EOF
run_root cp -f "${TMP_ENV}" "${PREFIX}/env.sh"
rm -f "${TMP_ENV}"

if [[ -f "${SCRIPT_DIR}/uninstall.sh" ]]; then
  run_root cp -f "${SCRIPT_DIR}/uninstall.sh" "${PREFIX}/bin/uninstall.sh"
  run_root chmod +x "${PREFIX}/bin/uninstall.sh"
fi
if [[ -f "${SCRIPT_DIR}/使い方.txt" ]]; then
  run_root cp -f "${SCRIPT_DIR}/使い方.txt" "${PREFIX}/使い方.txt"
fi

# 旧ホーム配置の残骸があれば案内
if [[ -d "${HOME}/OFX/Plugins/EmbrSAM2.ofx.bundle" ]]; then
  echo
  echo "注意: 以前の場所にもプラグインがあります:"
  echo "  ${HOME}/OFX/Plugins/EmbrSAM2.ofx.bundle"
  echo "ホストはここを見ないことが多いので、削除して構いません:"
  echo "  rm -rf \"${HOME}/OFX/Plugins/EmbrSAM2.ofx.bundle\""
fi
if [[ -d "${HOME}/EmbrSAM2" ]]; then
  echo
  echo "注意: 旧データ ${HOME}/EmbrSAM2 が残っています（新しい場所は ${PREFIX}）。"
fi

HAS_ENC=0
HAS_DEC=0
[[ -f "${PREFIX}/models/sam2/image_encoder.onnx" ]] && HAS_ENC=1
[[ -f "${PREFIX}/models/sam2/image_decoder.onnx" ]] && HAS_DEC=1

echo
echo "完了しました。"
echo
echo "【配置結果】"
echo "  ls ${OFX_DIR}/EmbrSAM2.ofx.bundle"
echo "  ls ${PREFIX}/models/sam2"
echo
echo "【次にやること】"
echo "1. Flame / Resolve を完全に終了して再起動"
echo "2. Batch の OpenFX ノード、または OFX エフェクト一覧を確認"
echo "   （表示名: Embr → EmbrSAM2）"
echo
if [[ "${HAS_ENC}" -ne 1 || "${HAS_DEC}" -ne 1 ]]; then
  echo "【モデル】まだ ONNX がありません。次を配置してください:"
  echo "  ${PREFIX}/models/sam2/image_encoder.onnx"
  echo "  ${PREFIX}/models/sam2/image_decoder.onnx"
  echo
else
  echo "【モデル】検出済み（デフォルトパスで利用可能）"
  echo
fi
echo "Flame で OpenFX 自体が出ない場合:"
echo "  /usr/OFX/Plugins にバンドルがあるか確認してください（必須パス）。"
echo "詳細: ${PREFIX}/使い方.txt"
