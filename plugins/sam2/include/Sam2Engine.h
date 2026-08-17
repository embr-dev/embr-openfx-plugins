#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace embr {

struct Sam2Point {
  float x = 0.f;  // pixel coords in source image space
  float y = 0.f;
  int label = 1;  // 1=foreground, 0=background
};

struct Sam2Box {
  float x0 = 0.f;
  float y0 = 0.f;
  float x1 = 0.f;
  float y1 = 0.f;
  bool enabled = false;
};

struct Sam2EngineConfig {
  std::string encoderPath;
  std::string decoderPath;
  std::string device = "auto";  // auto|cpu|cuda
  int threads = 4;
  int inputSize = 1024;
  float maskThreshold = 0.0f;
};

// Host-agnostic SAM2 image predictor (encoder + decoder ONNX).
class Sam2Engine {
 public:
  Sam2Engine();
  ~Sam2Engine();

  Sam2Engine(const Sam2Engine&) = delete;
  Sam2Engine& operator=(const Sam2Engine&) = delete;

  // Loads models. If paths are empty / missing, enters demo mode.
  bool load(const Sam2EngineConfig& config);
  bool isReady() const;
  bool isDemoMode() const;
  const std::string& lastError() const;
  const std::string& provider() const;

  // RGB planar or interleaved float image in 0..1, size width*height*3 interleaved RGB.
  // Returns single-channel soft mask (0..1) at source resolution.
  bool run(const float* rgbInterleaved,
           int width,
           int height,
           const std::vector<Sam2Point>& points,
           const Sam2Box& box,
           std::vector<float>& maskOut);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace embr
