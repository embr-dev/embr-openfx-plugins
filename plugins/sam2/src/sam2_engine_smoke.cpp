#include "Sam2Engine.h"

#include <cmath>
#include <iostream>
#include <vector>

int main() {
  embr::Sam2Engine engine;
  embr::Sam2EngineConfig cfg;
  cfg.encoderPath.clear();
  cfg.decoderPath.clear();
  if (!engine.load(cfg) || !engine.isReady()) {
    std::cerr << "load failed\n";
    return 1;
  }
  if (!engine.isDemoMode()) {
    std::cerr << "expected demo mode without models\n";
    return 1;
  }

  const int w = 64;
  const int h = 48;
  std::vector<float> rgb(static_cast<size_t>(w) * h * 3, 0.2f);
  embr::Sam2Box box;
  box.enabled = true;
  box.x0 = 16;
  box.y0 = 12;
  box.x1 = 48;
  box.y1 = 36;
  std::vector<float> mask;
  if (!engine.run(rgb.data(), w, h, {}, box, mask)) {
    std::cerr << "run failed: " << engine.lastError() << "\n";
    return 1;
  }
  if (mask.size() != static_cast<size_t>(w) * h) {
    std::cerr << "bad mask size\n";
    return 1;
  }
  float center = mask[static_cast<size_t>(24) * w + 32];
  float corner = mask[0];
  if (!(center > corner)) {
    std::cerr << "demo mask unexpected: center=" << center << " corner=" << corner << "\n";
    return 1;
  }
  std::cout << "sam2_engine_smoke OK provider=" << engine.provider()
            << " center=" << center << " corner=" << corner << "\n";
  return 0;
}
