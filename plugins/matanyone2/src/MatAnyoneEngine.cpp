#include "MatAnyoneEngine.h"

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

  // horizontal
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
  // vertical
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

  // Soft alpha: blurred dilation for fringe, hard eroded core.
  std::vector<float> alpha(mask.size());
  for (size_t i = 0; i < alpha.size(); ++i) {
    alpha[i] = std::max(eroded[i], dilated[i]);
  }
  boxBlur(alpha, w, h, std::max(1, blurR / 2));
  for (float& v : alpha) v = clampf(v, 0.f, 1.f);
  return alpha;
}

}  // namespace

struct MatAnyoneEngine::Impl {
  MatAnyoneEngineConfig config;
  bool ready = false;
  bool demoMode = true;
  std::string error;
  std::string backend = "none";

  int width = 0;
  int height = 0;
  bool hasPrev = false;
  std::vector<float> prevAlpha;
  std::vector<float> seedMask;
  bool hasSeed = false;
};

MatAnyoneEngine::MatAnyoneEngine() : impl_(std::make_unique<Impl>()) {}
MatAnyoneEngine::~MatAnyoneEngine() = default;

bool MatAnyoneEngine::isReady() const { return impl_ && impl_->ready; }
bool MatAnyoneEngine::isDemoMode() const { return impl_ && impl_->demoMode; }
const std::string& MatAnyoneEngine::lastError() const { return impl_->error; }
const std::string& MatAnyoneEngine::backend() const { return impl_->backend; }

void MatAnyoneEngine::reset() {
  if (!impl_) return;
  impl_->hasPrev = false;
  impl_->prevAlpha.clear();
  impl_->seedMask.clear();
  impl_->hasSeed = false;
  impl_->width = 0;
  impl_->height = 0;
}

bool MatAnyoneEngine::load(const MatAnyoneEngineConfig& config) {
  impl_->config = config;
  impl_->ready = true;
  impl_->error.clear();
  reset();

  if (!config.modelPath.empty() && fileExists(config.modelPath)) {
    impl_->demoMode = true;
    impl_->backend = "demo+weights-present";
    impl_->error =
        "matanyone2.pth found, but OFX neural backend is not enabled yet; using demo soft-matte.";
    std::cerr << "[embr-matanyone2] " << impl_->error << std::endl;
  } else {
    impl_->demoMode = true;
    impl_->backend = "demo";
    impl_->error = "Running demo soft-matte (neural MatAnyone2 backend pending).";
    std::cerr << "[embr-matanyone2] " << impl_->error << std::endl;
  }
  return true;
}

bool MatAnyoneEngine::step(const float* rgbInterleaved,
                           int width,
                           int height,
                           const float* maskIn,
                           bool hasMask,
                           std::vector<float>& alphaOut) {
  (void)rgbInterleaved;
  if (!impl_ || !impl_->ready) {
    impl_->error = "engine not ready";
    return false;
  }
  if (width <= 0 || height <= 0) {
    impl_->error = "invalid size";
    return false;
  }

  if (impl_->config.resetSequence || width != impl_->width || height != impl_->height) {
    reset();
    impl_->width = width;
    impl_->height = height;
  }

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
    for (size_t i = 0; i < n; ++i) {
      alpha[i] = (1.f - t) * alpha[i] + t * impl_->prevAlpha[i];
    }
  }

  impl_->prevAlpha = alpha;
  impl_->hasPrev = true;
  alphaOut.swap(alpha);
  impl_->error.clear();
  return true;
}

}  // namespace embr
