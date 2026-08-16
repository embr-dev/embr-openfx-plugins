# embr-openfx-plugins

Linux 向け OpenFX プラグイン集（社内検証用）。

OFX 標準パス: **`/usr/OFX/Plugins`**  
データ: **`/opt/Embr/<Product>/`**

---

## EmbrSAM2

マスク生成（点／箱プロンプト）。

```bash
./install.sh   # ZIP内。配置: /usr/OFX/Plugins + /opt/Embr/EmbrSAM2
```

## EmbrMatAnyone2

マスク → 柔らかいマット（SAM2 などと別ノード）。

```bash
# ビルド
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/plugins/matanyone2/matanyone2_engine_smoke
./scripts/package_matanyone2_installer_linux.sh

# インストール
unzip EmbrMatAnyone2-linux-x86_64-*.zip && cd EmbrMatAnyone2-linux-x86_64-*
./install.sh
```

| 内容 | パス |
|------|------|
| OFX | `/usr/OFX/Plugins/EmbrMatAnyone2.ofx.bundle` |
| Data | `/opt/Embr/EmbrMatAnyone2/` |

### 推奨配線

```text
[Source] ──┐
           ├─ EmbrMatAnyone2 → alpha / foreground
[EmbrSAM2 / Roto Mask] ─┘
```

### 現状（MatAnyone2 v0.1）

- OFX の入出力・デモ軟化マット・時間ブレンドが動作
- 公式神経モデル（`matanyone2.pth`）の **OFX 内推論は未接続**（状態付き PyTorch）
- 本番品質のオフライン推論: `scripts/run_matanyone2_offline.py`
- 重み取得: `./scripts/download_matanyone2_weights.sh` → インストーラー同梱可

### 次の実装予定

- MatAnyone2 の ONNX / LibTorch バックエンドを OFX Engine に接続

---

## ライセンス

- プラグイン骨格: LICENSE
- SAM2 / MatAnyone2 重み: 各上流ライセンスに従う
