#include "Sam2Engine.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <onnxruntime_cxx_api.h>

namespace embr {
namespace {

bool fileExists(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return f.good();
}

float sigmoid(float x) { return 1.f / (1.f + std::exp(-x)); }

std::vector<float> resizeRgbBilinear(const float* src, int sw, int sh, int dw, int dh) {
  std::vector<float> dst(static_cast<size_t>(dw) * dh * 3);
  const float xScale = sw > 1 ? static_cast<float>(sw - 1) / static_cast<float>(dw - 1) : 0.f;
  const float yScale = sh > 1 ? static_cast<float>(sh - 1) / static_cast<float>(dh - 1) : 0.f;
  for (int y = 0; y < dh; ++y) {
    const float sy = y * yScale;
    const int y0 = static_cast<int>(sy);
    const int y1 = std::min(y0 + 1, sh - 1);
    const float fy = sy - y0;
    for (int x = 0; x < dw; ++x) {
      const float sx = x * xScale;
      const int x0 = static_cast<int>(sx);
      const int x1 = std::min(x0 + 1, sw - 1);
      const float fx = sx - x0;
      for (int c = 0; c < 3; ++c) {
        const float v00 = src[(static_cast<size_t>(y0) * sw + x0) * 3 + c];
        const float v10 = src[(static_cast<size_t>(y0) * sw + x1) * 3 + c];
        const float v01 = src[(static_cast<size_t>(y1) * sw + x0) * 3 + c];
        const float v11 = src[(static_cast<size_t>(y1) * sw + x1) * 3 + c];
        const float v0 = v00 * (1.f - fx) + v10 * fx;
        const float v1 = v01 * (1.f - fx) + v11 * fx;
        dst[(static_cast<size_t>(y) * dw + x) * 3 + c] = v0 * (1.f - fy) + v1 * fy;
      }
    }
  }
  return dst;
}

std::vector<float> toNchwNormalized(const std::vector<float>& rgbHwC, int w, int h) {
  static const float kMean[3] = {0.485f, 0.456f, 0.406f};
  static const float kStd[3] = {0.229f, 0.224f, 0.225f};
  std::vector<float> out(static_cast<size_t>(3) * w * h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 3; ++c) {
        const float v = rgbHwC[(static_cast<size_t>(y) * w + x) * 3 + c];
        out[static_cast<size_t>(c) * w * h + static_cast<size_t>(y) * w + x] =
            (v - kMean[c]) / kStd[c];
      }
    }
  }
  return out;
}

std::vector<float> resizeMaskBilinear(const float* src, int sw, int sh, int dw, int dh) {
  std::vector<float> dst(static_cast<size_t>(dw) * dh);
  const float xScale = sw > 1 ? static_cast<float>(sw - 1) / static_cast<float>(dw - 1) : 0.f;
  const float yScale = sh > 1 ? static_cast<float>(sh - 1) / static_cast<float>(dh - 1) : 0.f;
  for (int y = 0; y < dh; ++y) {
    const float sy = y * yScale;
    const int y0 = static_cast<int>(sy);
    const int y1 = std::min(y0 + 1, sh - 1);
    const float fy = sy - y0;
    for (int x = 0; x < dw; ++x) {
      const float sx = x * xScale;
      const int x0 = static_cast<int>(sx);
      const int x1 = std::min(x0 + 1, sw - 1);
      const float fx = sx - x0;
      const float v00 = src[static_cast<size_t>(y0) * sw + x0];
      const float v10 = src[static_cast<size_t>(y0) * sw + x1];
      const float v01 = src[static_cast<size_t>(y1) * sw + x0];
      const float v11 = src[static_cast<size_t>(y1) * sw + x1];
      const float v0 = v00 * (1.f - fx) + v10 * fx;
      const float v1 = v01 * (1.f - fx) + v11 * fx;
      dst[static_cast<size_t>(y) * dw + x] = v0 * (1.f - fy) + v1 * fy;
    }
  }
  return dst;
}

std::vector<float> demoMask(int width,
                            int height,
                            const std::vector<Sam2Point>& points,
                            const Sam2Box& box) {
  std::vector<float> mask(static_cast<size_t>(width) * height, 0.f);
  float cx = width * 0.5f;
  float cy = height * 0.5f;
  float rx = width * 0.2f;
  float ry = height * 0.2f;
  if (box.enabled) {
    cx = 0.5f * (box.x0 + box.x1);
    cy = 0.5f * (box.y0 + box.y1);
    rx = std::max(1.f, 0.5f * std::abs(box.x1 - box.x0));
    ry = std::max(1.f, 0.5f * std::abs(box.y1 - box.y0));
  } else if (!points.empty()) {
    cx = points.front().x;
    cy = points.front().y;
  }
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float nx = (x - cx) / rx;
      const float ny = (y - cy) / ry;
      const float d = nx * nx + ny * ny;
      mask[static_cast<size_t>(y) * width + x] = std::clamp(1.f - d, 0.f, 1.f);
    }
  }
  return mask;
}

std::string pickName(const std::vector<std::string>& available,
                     std::initializer_list<const char*> candidates) {
  for (const char* c : candidates) {
    for (const auto& a : available) {
      if (a == c) return a;
    }
  }
  for (const char* c : candidates) {
    for (const auto& a : available) {
      if (a.find(c) != std::string::npos) return a;
    }
  }
  return available.empty() ? std::string() : available.front();
}

int indexOfName(const std::vector<std::string>& names, std::initializer_list<const char*> keys) {
  const std::string chosen = pickName(names, keys);
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i] == chosen) return static_cast<int>(i);
  }
  return names.empty() ? -1 : 0;
}

}  // namespace

struct Sam2Engine::Impl {
  Sam2EngineConfig config;
  bool ready = false;
  bool demoMode = false;
  std::string error;
  std::string provider = "none";

  Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "embr-sam2"};
  Ort::SessionOptions sessionOptions;
  Ort::AllocatorWithDefaultOptions allocator;
  std::unique_ptr<Ort::Session> encoder;
  std::unique_ptr<Ort::Session> decoder;
  Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

  std::vector<std::string> encInNames;
  std::vector<std::string> encOutNames;
  std::vector<std::string> decInNames;
  std::vector<std::string> decOutNames;
  std::mutex mutex;
};

Sam2Engine::Sam2Engine() : impl_(std::make_unique<Impl>()) {}
Sam2Engine::~Sam2Engine() = default;

bool Sam2Engine::isReady() const { return impl_ && impl_->ready; }
bool Sam2Engine::isDemoMode() const { return impl_ && impl_->demoMode; }
const std::string& Sam2Engine::lastError() const { return impl_->error; }
const std::string& Sam2Engine::provider() const { return impl_->provider; }

bool Sam2Engine::load(const Sam2EngineConfig& config) {
  impl_->config = config;
  impl_->ready = false;
  impl_->demoMode = false;
  impl_->error.clear();
  impl_->encoder.reset();
  impl_->decoder.reset();

  if (config.encoderPath.empty() || config.decoderPath.empty() || !fileExists(config.encoderPath) ||
      !fileExists(config.decoderPath)) {
    impl_->demoMode = true;
    impl_->ready = true;
    impl_->provider = "demo";
    impl_->error =
        "ONNX models not found; running in demo mode (elliptical mask from box/point).";
    std::cerr << "[embr-sam2] " << impl_->error << std::endl;
    return true;
  }

  try {
    impl_->sessionOptions = Ort::SessionOptions{};
    impl_->sessionOptions.SetIntraOpNumThreads(std::max(1, config.threads));
    impl_->sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    impl_->provider = "cpu";
#if defined(EMBR_SAM2_HAS_CUDA)
    if (config.device == "cuda" || config.device == "auto") {
      try {
        OrtCUDAProviderOptions cudaOpts{};
        cudaOpts.device_id = 0;
        impl_->sessionOptions.AppendExecutionProvider_CUDA(cudaOpts);
        impl_->provider = "cuda";
      } catch (const Ort::Exception& e) {
        if (config.device == "cuda") throw;
        std::cerr << "[embr-sam2] CUDA unavailable, using CPU: " << e.what() << std::endl;
        impl_->provider = "cpu";
        impl_->sessionOptions = Ort::SessionOptions{};
        impl_->sessionOptions.SetIntraOpNumThreads(std::max(1, config.threads));
        impl_->sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
      }
    }
#else
    if (config.device == "cuda") {
      std::cerr << "[embr-sam2] built without CUDA EP; using CPU" << std::endl;
    }
#endif

    impl_->encoder = std::make_unique<Ort::Session>(
        impl_->env, config.encoderPath.c_str(), impl_->sessionOptions);
    impl_->decoder = std::make_unique<Ort::Session>(
        impl_->env, config.decoderPath.c_str(), impl_->sessionOptions);

    auto collect = [&](Ort::Session& session, bool inputs) {
      std::vector<std::string> names;
      const size_t n = inputs ? session.GetInputCount() : session.GetOutputCount();
      for (size_t i = 0; i < n; ++i) {
        auto p = inputs ? session.GetInputNameAllocated(i, impl_->allocator)
                        : session.GetOutputNameAllocated(i, impl_->allocator);
        names.emplace_back(p.get());
      }
      return names;
    };

    impl_->encInNames = collect(*impl_->encoder, true);
    impl_->encOutNames = collect(*impl_->encoder, false);
    impl_->decInNames = collect(*impl_->decoder, true);
    impl_->decOutNames = collect(*impl_->decoder, false);
    impl_->ready = true;
    std::cerr << "[embr-sam2] ready provider=" << impl_->provider << std::endl;
    return true;
  } catch (const std::exception& e) {
    impl_->error = e.what();
    impl_->ready = false;
    std::cerr << "[embr-sam2] load failed: " << impl_->error << std::endl;
    return false;
  }
}

bool Sam2Engine::run(const float* rgbInterleaved,
                     int width,
                     int height,
                     const std::vector<Sam2Point>& points,
                     const Sam2Box& box,
                     std::vector<float>& maskOut) {
  if (!impl_ || !impl_->ready) {
    impl_->error = "engine not ready";
    return false;
  }
  if (!rgbInterleaved || width <= 0 || height <= 0) {
    impl_->error = "invalid image";
    return false;
  }

  if (impl_->demoMode) {
    maskOut = demoMask(width, height, points, box);
    return true;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  try {
    const int S = impl_->config.inputSize;
    auto resized = resizeRgbBilinear(rgbInterleaved, width, height, S, S);
    auto nchw = toNchwNormalized(resized, S, S);

    const std::string encIn =
        pickName(impl_->encInNames, {"image", "input", "images", "pixel_values"});
    std::vector<int64_t> encShape = {1, 3, S, S};
    Ort::Value encTensor = Ort::Value::CreateTensor<float>(
        impl_->memoryInfo, nchw.data(), nchw.size(), encShape.data(), encShape.size());

    std::vector<const char*> encInPtrs{encIn.c_str()};
    std::vector<const char*> encOutPtrs;
    for (auto& n : impl_->encOutNames) encOutPtrs.push_back(n.c_str());

    auto encOuts = impl_->encoder->Run(Ort::RunOptions{nullptr},
                                       encInPtrs.data(),
                                       &encTensor,
                                       1,
                                       encOutPtrs.data(),
                                       encOutPtrs.size());

    const int embedIdx =
        indexOfName(impl_->encOutNames, {"image_embed", "image_embeddings", "embed"});
    const int h0Idx =
        indexOfName(impl_->encOutNames, {"high_res_feats_0", "high_res_features_0", "feat_s0"});
    const int h1Idx =
        indexOfName(impl_->encOutNames, {"high_res_feats_1", "high_res_features_1", "feat_s1"});
    if (embedIdx < 0) throw std::runtime_error("missing image embedding output");

    std::vector<std::vector<float>> owned;
    std::vector<std::vector<int64_t>> ownedShapes;
    auto cloneTensor = [&](Ort::Value& v) -> size_t {
      auto shape = v.GetTensorTypeAndShapeInfo().GetShape();
      size_t count = 1;
      for (auto d : shape) count *= static_cast<size_t>(std::max<int64_t>(d, 1));
      const float* src = v.GetTensorData<float>();
      owned.emplace_back(src, src + count);
      ownedShapes.push_back(shape);
      return owned.size() - 1;
    };

    const size_t embedOwned = cloneTensor(encOuts[static_cast<size_t>(embedIdx)]);
    size_t h0Owned = static_cast<size_t>(-1);
    size_t h1Owned = static_cast<size_t>(-1);
    if (h0Idx >= 0 && h0Idx != embedIdx) h0Owned = cloneTensor(encOuts[static_cast<size_t>(h0Idx)]);
    if (h1Idx >= 0 && h1Idx != embedIdx && h1Idx != h0Idx) {
      h1Owned = cloneTensor(encOuts[static_cast<size_t>(h1Idx)]);
    }

    const float scaleX = static_cast<float>(S) / static_cast<float>(width);
    const float scaleY = static_cast<float>(S) / static_cast<float>(height);
    std::vector<float> coords;
    std::vector<float> labels;
    auto addPoint = [&](float x, float y, float label) {
      coords.push_back(x * scaleX);
      coords.push_back(y * scaleY);
      labels.push_back(label);
    };
    if (box.enabled) {
      addPoint(box.x0, box.y0, 2.f);
      addPoint(box.x1, box.y1, 3.f);
    }
    for (const auto& p : points) addPoint(p.x, p.y, static_cast<float>(p.label));
    if (coords.empty()) addPoint(width * 0.5f, height * 0.5f, 1.f);

    const int64_t numPoints = static_cast<int64_t>(labels.size());
    std::vector<int64_t> coordsShape = {1, numPoints, 2};
    std::vector<int64_t> labelsShape = {1, numPoints};
    std::vector<float> maskInput(256 * 256, 0.f);
    std::vector<float> hasMask = {0.f};
    std::vector<float> origHW = {static_cast<float>(height), static_cast<float>(width)};

    std::vector<Ort::Value> decInputs;
    std::vector<const char*> decInNamePtrs;
    for (const auto& name : impl_->decInNames) {
      decInNamePtrs.push_back(name.c_str());
      auto has = [&](const char* k) { return name.find(k) != std::string::npos; };

      if (has("point_coord") || has("coords")) {
        decInputs.push_back(Ort::Value::CreateTensor<float>(
            impl_->memoryInfo, coords.data(), coords.size(), coordsShape.data(), coordsShape.size()));
      } else if (has("point_label") || (has("label") && !has("has_mask"))) {
        decInputs.push_back(Ort::Value::CreateTensor<float>(
            impl_->memoryInfo, labels.data(), labels.size(), labelsShape.data(), labelsShape.size()));
      } else if (has("image_embed") || has("image_embeddings") || name == "embed") {
        decInputs.push_back(Ort::Value::CreateTensor<float>(impl_->memoryInfo,
                                                            owned[embedOwned].data(),
                                                            owned[embedOwned].size(),
                                                            ownedShapes[embedOwned].data(),
                                                            ownedShapes[embedOwned].size()));
      } else if (has("high_res") && (has("0") || has("s0") || has("feats_0") || has("features_0"))) {
        if (h0Owned == static_cast<size_t>(-1)) throw std::runtime_error("missing high_res_0");
        decInputs.push_back(Ort::Value::CreateTensor<float>(impl_->memoryInfo,
                                                            owned[h0Owned].data(),
                                                            owned[h0Owned].size(),
                                                            ownedShapes[h0Owned].data(),
                                                            ownedShapes[h0Owned].size()));
      } else if (has("high_res") && (has("1") || has("s1") || has("feats_1") || has("features_1"))) {
        if (h1Owned == static_cast<size_t>(-1)) throw std::runtime_error("missing high_res_1");
        decInputs.push_back(Ort::Value::CreateTensor<float>(impl_->memoryInfo,
                                                            owned[h1Owned].data(),
                                                            owned[h1Owned].size(),
                                                            ownedShapes[h1Owned].data(),
                                                            ownedShapes[h1Owned].size()));
      } else if (has("has_mask")) {
        std::vector<int64_t> shape = {1};
        decInputs.push_back(Ort::Value::CreateTensor<float>(
            impl_->memoryInfo, hasMask.data(), hasMask.size(), shape.data(), shape.size()));
      } else if (has("mask_input")) {
        std::vector<int64_t> shape = {1, 1, 256, 256};
        decInputs.push_back(Ort::Value::CreateTensor<float>(
            impl_->memoryInfo, maskInput.data(), maskInput.size(), shape.data(), shape.size()));
      } else if (has("orig")) {
        std::vector<int64_t> shape = {2};
        decInputs.push_back(Ort::Value::CreateTensor<float>(
            impl_->memoryInfo, origHW.data(), origHW.size(), shape.data(), shape.size()));
      } else {
        throw std::runtime_error("unhandled decoder input: " + name);
      }
    }

    std::vector<const char*> decOutNamePtrs;
    for (auto& n : impl_->decOutNames) decOutNamePtrs.push_back(n.c_str());
    auto decOuts = impl_->decoder->Run(Ort::RunOptions{nullptr},
                                       decInNamePtrs.data(),
                                       decInputs.data(),
                                       decInputs.size(),
                                       decOutNamePtrs.data(),
                                       decOutNamePtrs.size());

    const int masksIdx = indexOfName(impl_->decOutNames, {"masks", "mask", "low_res_masks", "output"});
    if (masksIdx < 0) throw std::runtime_error("decoder produced no masks");

    Ort::Value& masksVal = decOuts[static_cast<size_t>(masksIdx)];
    auto mShape = masksVal.GetTensorTypeAndShapeInfo().GetShape();
    const float* mData = masksVal.GetTensorData<float>();

    int mh = 0;
    int mw = 0;
    size_t planeOffset = 0;
    if (mShape.size() == 4) {
      mh = static_cast<int>(mShape[2]);
      mw = static_cast<int>(mShape[3]);
      planeOffset = 0;
    } else if (mShape.size() == 3) {
      mh = static_cast<int>(mShape[1]);
      mw = static_cast<int>(mShape[2]);
    } else if (mShape.size() == 2) {
      mh = static_cast<int>(mShape[0]);
      mw = static_cast<int>(mShape[1]);
    } else {
      throw std::runtime_error("unexpected mask rank");
    }

    std::vector<float> lowRes(static_cast<size_t>(mh) * mw);
    for (int i = 0; i < mh * mw; ++i) {
      float v = mData[planeOffset + static_cast<size_t>(i)];
      if (v < 0.f || v > 1.f) v = sigmoid(v);
      lowRes[static_cast<size_t>(i)] = v;
    }

    maskOut = resizeMaskBilinear(lowRes.data(), mw, mh, width, height);
    if (impl_->config.maskThreshold > 0.f) {
      for (float& v : maskOut) v = v >= impl_->config.maskThreshold ? 1.f : 0.f;
    }
    impl_->error.clear();
    return true;
  } catch (const std::exception& e) {
    impl_->error = e.what();
    std::cerr << "[embr-sam2] run failed: " << impl_->error << std::endl;
    maskOut = demoMask(width, height, points, box);
    return false;
  }
}

}  // namespace embr
