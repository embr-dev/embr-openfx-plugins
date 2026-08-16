#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace embr {

struct MatAnyoneEngineConfig {
  std::string modelPath;  // matanyone2.pth (future neural backend)
  std::string device = "auto";
  int erode = 10;
  int dilate = 10;
  float edgeSoftness = 2.0f;
  float temporalBlend = 0.35f;  // demo temporal smoothing
  bool resetSequence = false;
};

// Host-agnostic MatAnyone2-style video matting engine.
// Current build: high-quality demo soft-matte from first-frame / per-frame mask
// with temporal smoothing. Neural MatAnyone2 (.pth) hooks are reserved for a
// follow-up backend (LibTorch / ONNX export).
class MatAnyoneEngine {
 public:
  MatAnyoneEngine();
  ~MatAnyoneEngine();

  MatAnyoneEngine(const MatAnyoneEngine&) = delete;
  MatAnyoneEngine& operator=(const MatAnyoneEngine&) = delete;

  bool load(const MatAnyoneEngineConfig& config);
  bool isReady() const;
  bool isDemoMode() const;
  const std::string& lastError() const;
  const std::string& backend() const;

  void reset();

  // rgbInterleaved: RGB float 0..1, size width*height*3
  // maskIn: optional single-channel 0..1 (use on first frame, or every frame)
  // alphaOut: single-channel soft matte 0..1
  bool step(const float* rgbInterleaved,
            int width,
            int height,
            const float* maskIn,  // nullable
            bool hasMask,
            std::vector<float>& alphaOut);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace embr
