#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace embr {

struct MatAnyoneNeuralConfig {
  std::string pythonExe;
  std::string workerScript;
  std::string srcRoot;
  std::string modelPath;
  std::string device = "auto";
  int erode = 10;
  int dilate = 10;
  int warmup = 10;
  int maxSize = -1;
};

// Long-lived Python worker bridge for official MatAnyone2 (.pth) inference.
class MatAnyoneNeuralBackend {
 public:
  MatAnyoneNeuralBackend();
  ~MatAnyoneNeuralBackend();

  MatAnyoneNeuralBackend(const MatAnyoneNeuralBackend&) = delete;
  MatAnyoneNeuralBackend& operator=(const MatAnyoneNeuralBackend&) = delete;

  bool start(const MatAnyoneNeuralConfig& config);
  void stop();
  bool running() const;
  bool reset();
  bool step(const float* rgbInterleaved,
            int width,
            int height,
            const float* maskIn,
            bool hasMask,
            std::vector<float>& alphaOut);

  const std::string& lastError() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace embr
