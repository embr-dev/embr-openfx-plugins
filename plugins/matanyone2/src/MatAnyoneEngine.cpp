#include "MatAnyoneEngine.h"
#include "MatAnyoneNeuralBackend.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <vector>

namespace embr {
namespace {

bool fileExists(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return f.good();
}

float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

void boxBlur(std::vector<float>& img, int w, int h, int radius) {
  if (radius <= 0 || w <= 0 || h <= 0) return;
  std::vector<float> tmp(img.size());
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float sum = 0.f;
      int count = 0;
      for (int k = -radius; k <= radius; ++k) {
        const int xx = std::max(0, std::min(w - 1, x + k));
        sum += img[static_cast<size_t>(y) * w + xx];
        ++count;
      }
      tmp[static_cast<size_t>(y) * w + x] = sum / static_cast<float>(count);
    }
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float sum = 0.f;
      int count = 0;
      for (int k = -radius; k <= radius; ++k) {
        const int yy = std::max(0, std::min(h - 1, y + k));
        sum += tmp[static_cast<size_t>(yy) * w + x];
        ++count;
      }
      img[static_cast<size_t>(y) * w + x] = sum / static_cast<float>(count);
    }
  }
}

void morphMaxMin(std::vector<float>& img, int w, int h, int radius, bool dilate) {
  if (radius <= 0) return;
  std::vector<float> out(img.size());
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float v = dilate ? 0.f : 1.f;
      for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
          const int xx = std::max(0, std::min(w - 1, x + dx));
          const int yy = std::max(0, std::min(h - 1, y + dy));
          const float s = img[static_cast<size_t>(yy) * w + xx];
          v = dilate ? std::max(v, s) : std::min(v, s);
        }
      }
      out[static_cast<size_t>(y) * w + x] = v;
    }
  }
  img.swap(out);
}

std::vector<float> softMatteFromMask(const std::vector<float>& mask,
                                     int w,
                                     int h,
                                     int erode,
                                     int dilate,
                                     float softness) {
  std::vector<float> hard(mask.size());
  for (size_t i = 0; i < mask.size(); ++i) hard[i] = mask[i] >= 0.5f ? 1.f : 0.f;
  const int dR = std::max(0, std::min(dilate, 24) / 2);
  const int eR = std::max(0, std::min(erode, 24) / 2);
  std::vector<float> dilated = hard;
  std::vector<float> eroded = hard;
  morphMaxMin(dilated, w, h, dR, true);
  morphMaxMin(eroded, w, h, eR, false);
  const int blurR = std::max(1, static_cast<int>(std::lround(std::max(1.f, softness))));
  boxBlur(dilated, w, h, blurR);
  std::vector<float> alpha(mask.size());
  for (size_t i = 0; i < alpha.size(); ++i) alpha[i] = std::max(eroded[i], dilated[i]);
  boxBlur(alpha, w, h, std::max(1, blurR / 2));
  for (float& v : alpha) v = clampf(v, 0.f, 1.f);
  return alpha;
}

}  // namespace

struct MatAnyoneEngine::Impl {
  MatAnyoneEngineConfig config;
  bool ready = false;
  bool demoMode = true;
  bool neural = false;
  std::string error;
  std::string backend = "none";

  int width = 0;
  int height = 0;
  bool hasPrev = false;
  std::vector<float> prevAlpha;
  std::vector<float> seedMask;
  bool hasSeed = false;
  bool neuralSeeded = false;

  std::unique_ptr<MatAnyoneNeuralBackend> neuralBackend;
};

MatAnyoneEngine::MatAnyoneEngine() : impl_(std::make_unique<Impl>()) {}
MatAnyoneEngine::~MatAnyoneEngine() = default;

bool MatAnyoneEngine::isReady() const { return impl_ && impl_->ready; }
bool MatAnyoneEngine::isDemoMode() const { return impl_ && impl_->demoMode; }
bool MatAnyoneEngine::isNeural() const { return impl_ && impl_->neural; }
const std::string& MatAnyoneEngine::lastError() const { return impl_->error; }
const std::string& MatAnyoneEngine::backend() const { return impl_->backend; }

void MatAnyoneEngine::reset() {
  if (!impl_) return;
  impl_->hasPrev = false;
  impl_->prevAlpha.clear();
  impl_->seedMask.clear();
  impl_->hasSeed = false;
  impl_->neuralSeeded = false;
  impl_->width = 0;
  impl_->height = 0;
  if (impl_->neuralBackend && impl_->neuralBackend->running()) {
    impl_->neuralBackend->reset();
  }
}

bool MatAnyoneEngine::load(const MatAnyoneEngineConfig& config) {
  impl_->config = config;
  impl_->ready = true;
  impl_->error.clear();
  impl_->neural = false;
  impl_->demoMode = true;
  impl_->neuralBackend.reset();
  reset();

  const bool haveModel = !config.modelPath.empty() && fileExists(config.modelPath);
  const bool havePy = !config.pythonExe.empty() && fileExists(config.pythonExe);
  const bool haveWorker = !config.workerScript.empty() && fileExists(config.workerScript);
  const bool haveSrc = !config.srcRoot.empty() && fileExists(config.srcRoot + "/matanyone2");

  if (config.preferNeural && haveModel && havePy && haveWorker && haveSrc) {
    MatAnyoneNeuralConfig nc;
    nc.pythonExe = config.pythonExe;
    nc.workerScript = config.workerScript;
    nc.srcRoot = config.srcRoot;
    nc.modelPath = config.modelPath;
    nc.device = config.device;
    nc.erode = config.erode;
    nc.dilate = config.dilate;
    nc.warmup = config.warmup;
    nc.maxSize = config.maxSize;

    auto backend = std::make_unique<MatAnyoneNeuralBackend>();
    if (backend->start(nc)) {
      impl_->neuralBackend = std::move(backend);
      impl_->neural = true;
      impl_->demoMode = false;
      impl_->backend = "matanyone2-neural";
      std::cerr << "[embr-matanyone2] neural backend ready" << std::endl;
      return true;
    }
    impl_->error = backend->lastError();
    std::cerr << "[embr-matanyone2] neural start failed, fallback to demo: " << impl_->error
              << std::endl;
  } else if (config.preferNeural) {
    impl_->error = "neural prerequisites missing (model/python/worker/src); using demo";
    std::cerr << "[embr-matanyone2] " << impl_->error << std::endl;
  }

  impl_->demoMode = true;
  impl_->backend = haveModel ? "demo+weights-present" : "demo";
  return true;
}

bool MatAnyoneEngine::step(const float* rgbInterleaved,
                           int width,
                           int height,
                           const float* maskIn,
                           bool hasMask,
                           std::vector<float>& alphaOut) {
  if (!impl_ || !impl_->ready) {
    impl_->error = "engine not ready";
    return false;
  }
  if (width <= 0 || height <= 0) {
    impl_->error = "invalid size";
    return false;
  }

  if (impl_->config.resetSequence || width != impl_->width || height != impl_->height) {
    const bool dimChange = (width != impl_->width || height != impl_->height);
    if (dimChange || impl_->config.resetSequence) {
      reset();
    }
    impl_->width = width;
    impl_->height = height;
  }

  if (impl_->neural && impl_->neuralBackend) {
    // Seed only once (or after Reset). Hosts often keep Mask connected every frame.
    bool sendMask = false;
    if (!impl_->neuralSeeded) {
      if (!(hasMask && maskIn)) {
        alphaOut.assign(static_cast<size_t>(width) * height, 0.f);
        impl_->error = "waiting for mask input (neural seed)";
        return true;
      }
      sendMask = true;
    }

    if (!impl_->neuralBackend->step(
            rgbInterleaved, width, height, sendMask ? maskIn : nullptr, sendMask, alphaOut)) {
      impl_->error = impl_->neuralBackend->lastError();
      std::cerr << "[embr-matanyone2] neural step failed: " << impl_->error << std::endl;
      return false;
    }
    impl_->neuralSeeded = true;
    impl_->error.clear();
    return true;
  }

  // ---- demo fallback ----
  const size_t n = static_cast<size_t>(width) * height;
  if (hasMask && maskIn) {
    impl_->seedMask.assign(maskIn, maskIn + n);
    impl_->hasSeed = true;
  }
  if (!impl_->hasSeed) {
    alphaOut.assign(n, 0.f);
    impl_->error = "waiting for mask input";
    return true;
  }

  std::vector<float> alpha = softMatteFromMask(impl_->seedMask,
                                              width,
                                              height,
                                              impl_->config.erode,
                                              impl_->config.dilate,
                                              impl_->config.edgeSoftness);
  if (impl_->hasPrev && impl_->prevAlpha.size() == n && impl_->config.temporalBlend > 0.f) {
    const float t = clampf(impl_->config.temporalBlend, 0.f, 0.95f);
    for (size_t i = 0; i < n; ++i) alpha[i] = (1.f - t) * alpha[i] + t * impl_->prevAlpha[i];
  }
  impl_->prevAlpha = alpha;
  impl_->hasPrev = true;
  alphaOut.swap(alpha);
  return true;
}

}  // namespace embr
