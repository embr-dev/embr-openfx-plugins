# embr-openfx-plugins

Linux 向け OpenFX プラグイン集（`EmbrSAM2`, `EmbrMatAnyone2`）。C++17 + CMake でビルドし、
成果物は `*.ofx.bundle`（OpenFX ホスト＝DaVinci Resolve 等にロードされる共有ライブラリ）。

## Cursor Cloud specific instructions

このセクションは、更新スクリプト（ONNX Runtime の取得）が実行済みの環境で起動する
将来のエージェント向けの、非自明な注意点のみをまとめたもの。標準的なコマンドは
`README.md` に記載があるためそちらを参照。

### ブランチ構成（重要）
- 実際のコードは `main` ではなく `dev` ブランチ（および `cursor/*` フィーチャーブランチ）にある。
  `main` は `README.md` のみのほぼ空ブランチ。開発・ビルド・PR は `dev` を基点にすること。

### ビルド時の必須の注意点（gotcha）
- **必ず `g++` を明示指定すること。** デフォルトの `c++` は Clang 18 に解決され、
  `cannot find -lstdc++` で configure が失敗する。GNU ツールチェーンを使う:
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++
  cmake --build build -j
  ```
- ルートの `CMakeLists.txt` は `FindOnnxRuntime` を無条件に include するため、
  `EmbrMatAnyone2` だけをビルドする場合でも **ONNX Runtime が必要**
  （`third_party/onnxruntime/` に配置。更新スクリプトが自動取得する）。
  未取得なら手動で `bash scripts/download_onnxruntime_linux.sh`。
- `third_party/onnxruntime/` は `.gitignore` 済みで、fresh checkout には存在しない。
  起動時の更新スクリプトが取得を担う。
- OpenFX SDK は configure 時に CMake の FetchContent で GitHub から取得される
  （`build/_deps/openfx-src/`）。初回 configure はネットワークが必要。

### 実行 / 動作確認（テスト兼デモ）
- 本プロジェクトに正式な lint 設定（clang-format/clang-tidy）や ctest は無い。
  スモーク実行ファイルがテスト兼デモを兼ねる:
  ```bash
  ./build/plugins/matanyone2/matanyone2_engine_smoke   # → "... OK backend=demo ..."
  ./build/plugins/sam2/sam2_engine_smoke               # → "... OK provider=demo ..."
  ```
  重み／ONNX モデルが無い環境では両エンジンとも自動で demo モードで動作する（正常）。
- `EmbrSAM2.ofx` は `$ORIGIN` の rpath でバンドル同梱の `libonnxruntime.so.1` を解決する。
  ロード可否は `nm -D <bundle>/EmbrSAM2.ofx | grep OfxGetPlugin` で確認できる。

### オプション（デフォルトでは未セットアップ）
- `scripts/run_matanyone2_offline.py` / `scripts/export_sam2_onnx.py` は
  PyTorch と上流モデル（大容量・GPU 前提）が必要な将来用スクリプト。通常の環境構築対象外。
