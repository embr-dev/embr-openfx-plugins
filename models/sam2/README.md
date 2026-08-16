# SAM2 ONNX models (git には通常コミットしない)

取得:

```bash
./scripts/download_sam2_models.sh
# 大きいモデル: SAM2_MODEL_SIZE=base_plus ./scripts/download_sam2_models.sh
```

配置されるファイル:

- `image_encoder.onnx`
- `image_decoder.onnx`
- `SOURCE.txt`（出典）

`./scripts/package_installer_linux.sh` 実行時にインストーラー ZIP へ同梱されます。
インストール後は `~/EmbrSAM2/models/sam2/` にコピーされます。
