# embr-openfx-plugins

Linux 向け OpenFX プラグイン集（社内検証用）。

## 利用者向け（知識不要）

ビルド済み ZIP がある場合:

```bash
unzip EmbrSAM2-linux-x86_64-0.1.0.zip
cd EmbrSAM2-linux-x86_64-0.1.0
./install.sh
```

詳しくは ZIP 内の `使い方.txt` を参照。

開発者が ZIP を作る場合:

```bash
./scripts/download_onnxruntime_linux.sh
./scripts/download_sam2_models.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./scripts/package_installer_linux.sh
# → dist/EmbrSAM2-linux-x86_64-0.1.0.zip（モデル同梱）
```

## EmbrSAM2（Linux）

Meta SAM2 の **画像モード**（encoder + decoder ONNX）を OpenFX フィルタとして提供します。

- ホスト: Flame / Resolve / Natron など OFX 対応アプリ（Linux x86_64）
- 推論: ONNX Runtime（CPU 標準。GPU パッケージがあれば CUDA EP を有効化）
- モデル未配置時: **demo mode**（点／箱から楕円マスク）でホスト接続だけ先行検証可能
- インストール後の標準配置:
  - プラグイン: `~/OFX/Plugins/EmbrSAM2.ofx.bundle`（ORT 同梱）
  - モデル: `~/EmbrSAM2/models/sam2/`

### ビルド

```bash
chmod +x scripts/download_onnxruntime_linux.sh
./scripts/download_onnxruntime_linux.sh

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/plugins/sam2/sam2_engine_smoke
./scripts/package_installer_linux.sh
```

### モデル

```bash
python3 scripts/export_sam2_onnx.py --via hint
# image_encoder.onnx / image_decoder.onnx を
#   ~/EmbrSAM2/models/sam2/
# または ZIP 作成前に models/sam2/ へ置くと同梱されます
```

### パラメータ

| 項目 | 内容 |
|------|------|
| Encoder/Decoder ONNX | デフォルト `~/EmbrSAM2/models/sam2/*.onnx` |
| Device | auto / cuda / cpu |
| Use Box + Box X0..Y1 | 正規化 0..1 の矩形プロンプト |
| Point X/Y + Label | 正規化座標の点プロンプト |
| Output | Alpha / Mask RGB / Foreground |
| Reload Models | パス変更後の再読込 |

### 現状の範囲

- **画像プロンプト（点・箱）→ マスク** まで
- 動画メモリ伝播（SAM2 video）は未実装
- MatAnyone 連携はマスク出力を次ノードへ渡す想定

### ライセンス注意

- 本リポジトリのプラグイン骨格: LICENSE に従う
- SAM2 重み・ONNX: Meta SAM2 および export 元のライセンスに従う
- ONNX Runtime: Microsoft のライセンスに従う
