# embr-openfx-plugins

Linux 向け OpenFX プラグイン集（社内検証用）。

## 利用者向け（知識不要）

```bash
unzip EmbrSAM2-linux-x86_64-*.zip
cd EmbrSAM2-linux-x86_64-*
./install.sh    # sudo が必要
```

### 正しいインストール先

| 内容 | パス |
|------|------|
| OFX プラグイン | `/usr/OFX/Plugins/EmbrSAM2.ofx.bundle` |
| モデル・設定 | `/opt/Embr/EmbrSAM2/` |

Linux の OFX 標準検索パスは **`/usr/OFX/Plugins`** です（`~/OFX/Plugins` では Flame / Resolve に出ません）。  
Flame は `/usr/OFX/Plugins` にプラグインが無いと OpenFX ノード自体が出ないことがあります。

詳しくは ZIP 内の `使い方.txt`。

### ZIP を作る（開発者）

```bash
./scripts/download_onnxruntime_linux.sh
./scripts/download_sam2_models.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./scripts/package_installer_linux.sh
# → dist/EmbrSAM2-linux-x86_64-0.1.1.zip
```

## EmbrSAM2（Linux）

- ホスト: Flame / Resolve / Natron（Linux x86_64）
- 推論: ONNX Runtime（バンドル同梱）
- モデル未配置時: demo mode

### パラメータ

| 項目 | 内容 |
|------|------|
| Encoder/Decoder ONNX | デフォルト `/opt/Embr/EmbrSAM2/models/sam2/*.onnx` |
| Device | auto / cuda / cpu |
| Use Box / Point | 正規化 0..1 プロンプト |
| Output | Alpha / Mask RGB / Foreground |

### ライセンス

- プラグイン骨格: LICENSE
- SAM2 重み: Meta SAM 2 / 配布元に従う
- ONNX Runtime: Microsoft に従う
