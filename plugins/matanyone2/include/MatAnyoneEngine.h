#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace embr {

struct MatAnyoneEngineConfig {
  std::string modelPath;     // matanyone2.pth
  std::string pythonExe;     // venv python
  std::string workerScript;  // matanyone2_worker.py
  std::string srcRoot;       // MatAnyone2 source root (contains matanyone2/)
  std::string device = "auto";
  int erode = 10;
  int dilate = 10;
  int warmup = 10;
  int maxSize = -1;
  float edgeSoftness = 2.0f;   // demo only
  float temporalBlend = 0.0f;  // demo only
  bool resetSequence = false;
  bool preferNeural = true;
};

class MatAnyoneEngine {
 public:
  MatAnyoneEngine();
  ~MatAnyoneEngine();

  MatAnyoneEngine(const MatAnyoneEngine&) = delete;
  MatAnyoneEngine& operator=(const MatAnyoneEngine&) = delete;

  bool load(const MatAnyoneEngineConfig& config);
  bool isReady() const;
  bool isDemoMode() const;
  bool isNeural() const;
  const std::string& lastError() const;
  const std::string& backend() const;

  void reset();

  bool step(const float* rgbInterleaved,
            int width,
            int height,
            const float* maskIn,
            bool hasMask,
            std::vector<float>& alphaOut);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace embr
