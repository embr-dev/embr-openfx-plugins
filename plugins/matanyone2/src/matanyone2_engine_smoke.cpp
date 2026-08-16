#include "MatAnyoneEngine.h"

#include <iostream>
#include <vector>

int main() {
  embr::MatAnyoneEngine engine;
  embr::MatAnyoneEngineConfig cfg;
  if (!engine.load(cfg) || !engine.isReady()) {
    std::cerr << "load failed\n";
    return 1;
  }

  const int w = 64;
  const int h = 48;
  std::vector<float> rgb(static_cast<size_t>(w) * h * 3, 0.3f);
  std::vector<float> mask(static_cast<size_t>(w) * h, 0.f);
  for (int y = 12; y < 36; ++y) {
    for (int x = 16; x < 48; ++x) {
      mask[static_cast<size_t>(y) * w + x] = 1.f;
    }
  }

  std::vector<float> alpha;
  if (!engine.step(rgb.data(), w, h, mask.data(), true, alpha)) {
    std::cerr << "step failed: " << engine.lastError() << "\n";
    return 1;
  }
  if (alpha.size() != mask.size()) {
    std::cerr << "bad alpha size\n";
    return 1;
  }
  const float center = alpha[static_cast<size_t>(24) * w + 32];
  const float corner = alpha[0];
  if (!(center > corner)) {
    std::cerr << "unexpected matte center=" << center << " corner=" << corner << "\n";
    return 1;
  }
  std::cout << "matanyone2_engine_smoke OK backend=" << engine.backend()
            << " center=" << center << " corner=" << corner << "\n";
  return 0;
}
