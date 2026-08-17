#include "MatAnyoneEngine.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// Neural smoke against local venv + vendored MatAnyone2 + weights.
int main() {
  const char* home = std::getenv("EMBR_MATANYONE2_HOME");
  const std::string root = home ? home : "/workspace";

  embr::MatAnyoneEngineConfig cfg;
  cfg.preferNeural = true;
  cfg.modelPath = root + "/models/matanyone2/matanyone2.pth";
  cfg.pythonExe = root + "/.venv-matanyone/bin/python";
  cfg.workerScript = root + "/plugins/matanyone2/python/matanyone2_worker.py";
  cfg.srcRoot = root + "/third_party/MatAnyone2";
  cfg.device = "cpu";
  cfg.erode = 2;
  cfg.dilate = 2;
  cfg.warmup = 2;
  cfg.maxSize = -1;

  embr::MatAnyoneEngine engine;
  if (!engine.load(cfg)) {
    std::cerr << "load failed: " << engine.lastError() << "\n";
    return 1;
  }
  if (!engine.isNeural()) {
    std::cerr << "expected neural backend, got " << engine.backend()
              << " err=" << engine.lastError() << "\n";
    return 2;
  }

  const int w = 96;
  const int h = 96;
  std::vector<float> rgb(static_cast<size_t>(w) * h * 3, 0.35f);
  std::vector<float> mask(static_cast<size_t>(w) * h, 0.f);
  // Bright subject square + matching mask
  for (int y = 24; y < 72; ++y) {
    for (int x = 24; x < 72; ++x) {
      const size_t i = static_cast<size_t>(y) * w + x;
      mask[i] = 1.f;
      rgb[i * 3 + 0] = 0.85f;
      rgb[i * 3 + 1] = 0.75f;
      rgb[i * 3 + 2] = 0.65f;
    }
  }

  std::vector<float> alpha;
  if (!engine.step(rgb.data(), w, h, mask.data(), true, alpha)) {
    std::cerr << "step0 failed: " << engine.lastError() << "\n";
    return 3;
  }
  float sum0 = 0.f;
  for (float v : alpha) sum0 += v;

  // Slightly vary frame 1 (propagate / warmup)
  for (size_t i = 0; i < rgb.size(); ++i) rgb[i] = std::min(1.f, rgb[i] + 0.02f);
  if (!engine.step(rgb.data(), w, h, nullptr, false, alpha)) {
    std::cerr << "step1 failed: " << engine.lastError() << "\n";
    return 4;
  }
  float sum1 = 0.f;
  for (float v : alpha) sum1 += v;

  if (sum0 < 1.f) {
    std::cerr << "step0 alpha too small: " << sum0 << "\n";
    return 5;
  }

  std::cout << "matanyone2_neural_smoke OK backend=" << engine.backend()
            << " alpha_sum0=" << sum0 << " alpha_sum1=" << sum1 << "\n";
  return 0;
}
