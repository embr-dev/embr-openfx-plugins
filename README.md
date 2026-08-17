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

マスク → 柔らかいマット（SAM2 などと別ノード）。公式 `matanyone2.pth` による本推論対応。

```bash
# 依存
./scripts/download_matanyone2_weights.sh
git clone --depth 1 https://github.com/pq-yang/MatAnyone2.git third_party/MatAnyone2

# ビルド
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
./build/plugins/matanyone2/matanyone2_engine_smoke
./build/plugins/matanyone2/matanyone2_neural_smoke   # venv + weights 必要
./scripts/package_matanyone2_installer_linux.sh

# インストール
unzip EmbrMatAnyone2-linux-x86_64-*.zip && cd EmbrMatAnyone2-linux-x86_64-*
./install.sh   # OFX + models + Python worker + venv
```

| 内容 | パス |
|------|------|
| OFX | `/usr/OFX/Plugins/EmbrMatAnyone2.ofx.bundle` |
| Data | `/opt/Embr/EmbrMatAnyone2/`（models / python / src / venv） |

### 推奨配線

```text
[Source] ──┐
           ├─ EmbrMatAnyone2 → alpha / foreground
[EmbrSAM2 / Roto Mask] ─┘
```

### 現状（MatAnyone2 v0.1.1）

- Prefer Neural=ON かつ model + venv + src が揃うと **公式本推論**
- 欠けている場合は demo soft-matte にフォールバック
- Mask はシード用（初回／Reset 後）。常時接続でも毎フレーム再シードしない
- オフライン CLI: `scripts/run_matanyone2_offline.py`

---

## ライセンス

- プラグイン骨格: LICENSE
- SAM2 / MatAnyone2 重み: 各上流ライセンスに従う
