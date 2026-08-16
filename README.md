# embr-openfx-plugins

Linux 向け OpenFX プラグイン集（社内検証用）。

## EmbrSAM2（Linux）

Meta SAM2 の **画像モード**（encoder + decoder ONNX）を OpenFX フィルタとして提供します。

- ホスト: Flame / Resolve / Natron など OFX 対応アプリ（Linux x86_64）
- 推論: ONNX Runtime（CPU 標準。GPU パッケージがあれば CUDA EP を有効化）
- モデル未配置時: **demo mode**（点／箱から楕円マスク）でホスト接続だけ先行検証可能

### ビルド

```bash
# 依存取得
chmod +x scripts/download_onnxruntime_linux.sh
./scripts/download_onnxruntime_linux.sh

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# スモークテスト（demo mode）
./build/plugins/sam2/sam2_engine_smoke
```

成果物:

```text
build/ofx/EmbrSAM2.ofx.bundle/Contents/Linux-x86-64/EmbrSAM2.ofx
```

### ホストへの配置例

```bash
mkdir -p "$HOME/OFX/Plugins"
cp -a build/ofx/EmbrSAM2.ofx.bundle "$HOME/OFX/Plugins/"
# Flame は OFX プラグイン検索パス設定に従って配置
export LD_LIBRARY_PATH="/path/to/third_party/onnxruntime/lib:${LD_LIBRARY_PATH}"
```

`libonnxruntime.so` がプラグインと同じマシンで見つかるようにしてください。

### モデル

```bash
python3 scripts/export_sam2_onnx.py --via hint
# 推奨: sam2-onnx-cpp で export した
#   image_encoder.onnx / image_decoder.onnx
# を models/sam2/ へ配置
```

OFX UI の Encoder/Decoder パス、またはデフォルト `models/sam2/*.onnx` を指定します。

### パラメータ

| 項目 | 内容 |
|------|------|
| Encoder/Decoder ONNX | モデルパス |
| Device | auto / cuda / cpu |
| Use Box + Box X0..Y1 | 正規化 0..1 の矩形プロンプト |
| Point X/Y + Label | 正規化座標の点プロンプト |
| Output | Alpha / Mask RGB / Foreground |
| Reload Models | パス変更後の再読込 |

### 現状の範囲

- **画像プロンプト（点・箱）→ マスク** まで
- 動画メモリ伝播（SAM2 video）は未実装（別ノード／後続）
- MatAnyone 連携はマスク出力を次ノードへ渡す想定

### ライセンス注意

- 本リポジトリのプラグイン骨格: リポジトリの LICENSE に従う
- SAM2 重み・ONNX: Meta SAM2 および利用する export 元のライセンスに従う
- ONNX Runtime: Microsoft のライセンスに従う
