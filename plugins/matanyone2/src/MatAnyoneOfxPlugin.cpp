#include "MatAnyoneEngine.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"

namespace {

constexpr const char* kPluginName = "EmbrMatAnyone2";
constexpr const char* kPluginGrouping = "Embr";
constexpr const char* kPluginDescription =
    "MatAnyone2 video matting (Linux OFX). Source + Mask -> soft alpha / foreground. "
    "Uses official matanyone2.pth via Python worker when installed; otherwise demo soft-matte.";
constexpr const char* kPluginIdentifier = "jp.embr.ofx.MatAnyone2";
constexpr unsigned int kPluginVersionMajor = 0;
constexpr unsigned int kPluginVersionMinor = 2;

constexpr const char* kClipMask = "Mask";
constexpr const char* kParamModelPath = "modelPath";
constexpr const char* kParamPythonExe = "pythonExe";
constexpr const char* kParamWorkerScript = "workerScript";
constexpr const char* kParamSrcRoot = "srcRoot";
constexpr const char* kParamDevice = "device";
constexpr const char* kParamErode = "erode";
constexpr const char* kParamDilate = "dilate";
constexpr const char* kParamWarmup = "warmup";
constexpr const char* kParamMaxSize = "maxSize";
constexpr const char* kParamSoftness = "edgeSoftness";
constexpr const char* kParamTemporal = "temporalBlend";
constexpr const char* kParamPreferNeural = "preferNeural";
constexpr const char* kParamOutputMode = "outputMode";
constexpr const char* kParamReset = "resetSequence";
constexpr const char* kParamReload = "reload";

constexpr const char* kDefaultHome = "/opt/Embr/EmbrMatAnyone2";
constexpr const char* kDefaultModelPath = "/opt/Embr/EmbrMatAnyone2/models/matanyone2.pth";
constexpr const char* kDefaultPython = "/opt/Embr/EmbrMatAnyone2/venv/bin/python";
constexpr const char* kDefaultWorker = "/opt/Embr/EmbrMatAnyone2/python/matanyone2_worker.py";
constexpr const char* kDefaultSrc = "/opt/Embr/EmbrMatAnyone2/src/MatAnyone2";

enum OutputModeEnum {
  eOutputMaskAsAlpha = 0,
  eOutputMaskAsRgb,
  eOutputForeground
};

std::string productHome() {
  const char* rootEnv = std::getenv("EMBR_MATANYONE2_HOME");
  return rootEnv ? rootEnv : kDefaultHome;
}

std::string expandUserPath(std::string path) {
  if (path.empty()) return path;
  const char* home = std::getenv("HOME");
  const std::string root = productHome();
  if (path.rfind("$EMBR_MATANYONE2_HOME", 0) == 0) {
    path.replace(0, std::string("$EMBR_MATANYONE2_HOME").size(), root);
  }
  if (home && !path.empty() && path[0] == '~') {
    path.replace(0, 1, home);
  }
  return path;
}

class MatAnyoneProcessor : public OFX::ImageProcessor {
 public:
  explicit MatAnyoneProcessor(OFX::ImageEffect& effect)
      : OFX::ImageProcessor(effect), _srcImg(nullptr), _alpha(nullptr) {}

  void setSrcImg(OFX::Image* img) { _srcImg = img; }
  void setAlpha(const std::vector<float>* alpha, int w, int h) {
    _alpha = alpha;
    _w = w;
    _h = h;
  }
  void setOutputMode(OutputModeEnum mode) { _mode = mode; }

  void multiThreadProcessImages(OfxRectI procWindow) override {
    if (!_srcImg || !_dstImg || !_alpha) return;
    const OfxRectI& bounds = _srcImg->getBounds();
    for (int y = procWindow.y1; y < procWindow.y2; ++y) {
      if (_effect.abort()) break;
      for (int x = procWindow.x1; x < procWindow.x2; ++x) {
        float* dst = static_cast<float*>(_dstImg->getPixelAddress(x, y));
        const float* src = static_cast<const float*>(_srcImg->getPixelAddress(x, y));
        if (!dst || !src) continue;
        const int mx = std::clamp(x - bounds.x1, 0, _w - 1);
        const int my = std::clamp(y - bounds.y1, 0, _h - 1);
        const float a = (*_alpha)[static_cast<size_t>(my) * _w + mx];
        switch (_mode) {
          case eOutputMaskAsRgb:
            dst[0] = dst[1] = dst[2] = a;
            dst[3] = 1.f;
            break;
          case eOutputForeground:
            dst[0] = src[0] * a;
            dst[1] = src[1] * a;
            dst[2] = src[2] * a;
            dst[3] = a;
            break;
          case eOutputMaskAsAlpha:
          default:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = a;
            break;
        }
      }
    }
  }

 private:
  OFX::Image* _srcImg;
  const std::vector<float>* _alpha;
  int _w = 0;
  int _h = 0;
  OutputModeEnum _mode = eOutputMaskAsAlpha;
};

class MatAnyonePlugin : public OFX::ImageEffect {
 public:
  explicit MatAnyonePlugin(OfxImageEffectHandle handle)
      : ImageEffect(handle), _loaded(false) {
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);
    _maskClip = fetchClip(kClipMask);

    _modelPath = fetchStringParam(kParamModelPath);
    _pythonExe = fetchStringParam(kParamPythonExe);
    _workerScript = fetchStringParam(kParamWorkerScript);
    _srcRoot = fetchStringParam(kParamSrcRoot);
    _device = fetchStringParam(kParamDevice);
    _erode = fetchIntParam(kParamErode);
    _dilate = fetchIntParam(kParamDilate);
    _warmup = fetchIntParam(kParamWarmup);
    _maxSize = fetchIntParam(kParamMaxSize);
    _softness = fetchDoubleParam(kParamSoftness);
    _temporal = fetchDoubleParam(kParamTemporal);
    _preferNeural = fetchBooleanParam(kParamPreferNeural);
    _outputMode = fetchChoiceParam(kParamOutputMode);
    _reset = fetchPushButtonParam(kParamReset);
    _reload = fetchPushButtonParam(kParamReload);
  }

  void render(const OFX::RenderArguments& args) override {
    std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
    if (!dst.get() || !src.get()) {
      OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    ensureLoaded();

    const OfxRectI bounds = src->getBounds();
    const int width = bounds.x2 - bounds.x1;
    const int height = bounds.y2 - bounds.y1;
    if (width <= 0 || height <= 0) return;

    std::vector<float> rgb(static_cast<size_t>(width) * height * 3);
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const float* p =
            static_cast<const float*>(src->getPixelAddress(bounds.x1 + x, bounds.y1 + y));
        float* o = &rgb[(static_cast<size_t>(y) * width + x) * 3];
        if (!p) {
          o[0] = o[1] = o[2] = 0.f;
        } else {
          o[0] = p[0];
          o[1] = p[1];
          o[2] = p[2];
        }
      }
    }

    bool hasMask = false;
    std::vector<float> mask(static_cast<size_t>(width) * height, 0.f);
    if (_maskClip && _maskClip->isConnected()) {
      std::unique_ptr<OFX::Image> mimg(_maskClip->fetchImage(args.time));
      if (mimg.get()) {
        hasMask = true;
        const OfxRectI mb = mimg->getBounds();
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width; ++x) {
            const int sx = std::clamp(mb.x1 + x, mb.x1, mb.x2 - 1);
            const int sy = std::clamp(mb.y1 + y, mb.y1, mb.y2 - 1);
            const float* p = static_cast<const float*>(mimg->getPixelAddress(sx, sy));
            float v = 0.f;
            if (p) {
              v = (p[3] > 1e-6f) ? p[3] : (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]);
            }
            mask[static_cast<size_t>(y) * width + x] = v;
          }
        }
      }
    }

    std::vector<float> alpha;
    _engine.step(rgb.data(), width, height, hasMask ? mask.data() : nullptr, hasMask, alpha);
    if (alpha.size() != static_cast<size_t>(width) * height) {
      alpha.assign(static_cast<size_t>(width) * height, 0.f);
    }

    int mode = 0;
    _outputMode->getValueAtTime(args.time, mode);

    MatAnyoneProcessor processor(*this);
    processor.setDstImg(dst.get());
    processor.setSrcImg(src.get());
    processor.setAlpha(&alpha, width, height);
    processor.setOutputMode(static_cast<OutputModeEnum>(mode));
    processor.setRenderWindow(args.renderWindow);
    processor.process();
  }

  bool isIdentity(const OFX::IsIdentityArguments&, OFX::Clip*&, double&) override {
    return false;
  }

  void changedParam(const OFX::InstanceChangedArgs&, const std::string& name) override {
    if (name == kParamReload || name == kParamModelPath || name == kParamPythonExe ||
        name == kParamWorkerScript || name == kParamSrcRoot || name == kParamDevice ||
        name == kParamPreferNeural || name == kParamWarmup || name == kParamMaxSize) {
      _loaded = false;
    }
    if (name == kParamReset) {
      _engine.reset();
    }
  }

 private:
  void ensureLoaded() {
    if (_loaded) return;
    embr::MatAnyoneEngineConfig cfg;
    _modelPath->getValue(cfg.modelPath);
    _pythonExe->getValue(cfg.pythonExe);
    _workerScript->getValue(cfg.workerScript);
    _srcRoot->getValue(cfg.srcRoot);
    _device->getValue(cfg.device);
    cfg.modelPath = expandUserPath(cfg.modelPath);
    cfg.pythonExe = expandUserPath(cfg.pythonExe);
    cfg.workerScript = expandUserPath(cfg.workerScript);
    cfg.srcRoot = expandUserPath(cfg.srcRoot);

    int erode = 10, dilate = 10, warmup = 10, maxSize = -1;
    double soft = 2.0, temporal = 0.0;
    bool prefer = true;
    _erode->getValue(erode);
    _dilate->getValue(dilate);
    _warmup->getValue(warmup);
    _maxSize->getValue(maxSize);
    _softness->getValue(soft);
    _temporal->getValue(temporal);
    _preferNeural->getValue(prefer);
    cfg.erode = erode;
    cfg.dilate = dilate;
    cfg.warmup = warmup;
    cfg.maxSize = maxSize;
    cfg.edgeSoftness = static_cast<float>(soft);
    cfg.temporalBlend = static_cast<float>(temporal);
    cfg.preferNeural = prefer;
    _engine.load(cfg);
    _loaded = true;
  }

  OFX::Clip* _dstClip = nullptr;
  OFX::Clip* _srcClip = nullptr;
  OFX::Clip* _maskClip = nullptr;
  OFX::StringParam* _modelPath = nullptr;
  OFX::StringParam* _pythonExe = nullptr;
  OFX::StringParam* _workerScript = nullptr;
  OFX::StringParam* _srcRoot = nullptr;
  OFX::StringParam* _device = nullptr;
  OFX::IntParam* _erode = nullptr;
  OFX::IntParam* _dilate = nullptr;
  OFX::IntParam* _warmup = nullptr;
  OFX::IntParam* _maxSize = nullptr;
  OFX::DoubleParam* _softness = nullptr;
  OFX::DoubleParam* _temporal = nullptr;
  OFX::BooleanParam* _preferNeural = nullptr;
  OFX::ChoiceParam* _outputMode = nullptr;
  OFX::PushButtonParam* _reset = nullptr;
  OFX::PushButtonParam* _reload = nullptr;

  embr::MatAnyoneEngine _engine;
  bool _loaded;
};

class MatAnyonePluginFactory : public OFX::PluginFactoryHelper<MatAnyonePluginFactory> {
 public:
  MatAnyonePluginFactory()
      : OFX::PluginFactoryHelper<MatAnyonePluginFactory>(
            kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor) {}

  void describe(OFX::ImageEffectDescriptor& desc) override {
    desc.setLabels(kPluginName, kPluginName, kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);
    desc.addSupportedContext(OFX::eContextFilter);
    desc.addSupportedContext(OFX::eContextGeneral);
    desc.addSupportedBitDepth(OFX::eBitDepthFloat);
    desc.setSupportsTiles(false);
    desc.setSupportsMultiResolution(false);
    desc.setRenderThreadSafety(OFX::eRenderInstanceSafe);
  }

  void describeInContext(OFX::ImageEffectDescriptor& desc, OFX::ContextEnum) override {
    OFX::ClipDescriptor* src = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    src->addSupportedComponent(OFX::ePixelComponentRGBA);
    src->setSupportsTiles(false);

    OFX::ClipDescriptor* mask = desc.defineClip(kClipMask);
    mask->addSupportedComponent(OFX::ePixelComponentRGBA);
    mask->setOptional(true);
    mask->setSupportsTiles(false);

    OFX::ClipDescriptor* dst = desc.defineClip(kOfxImageEffectOutputClipName);
    dst->addSupportedComponent(OFX::ePixelComponentRGBA);
    dst->setSupportsTiles(false);

    OFX::PageParamDescriptor* page = desc.definePageParam("Controls");

    {
      auto* p = desc.defineStringParam(kParamModelPath);
      p->setLabels("Model (.pth)", "Model", "Model");
      p->setStringType(OFX::eStringTypeFilePath);
      p->setDefault(kDefaultModelPath);
      p->setHint("Official matanyone2.pth");
      page->addChild(*p);
    }
    {
      auto* p = desc.defineStringParam(kParamPythonExe);
      p->setLabels("Python (venv)", "Python", "Python");
      p->setStringType(OFX::eStringTypeFilePath);
      p->setDefault(kDefaultPython);
      p->setHint("venv python with torch + MatAnyone2 deps");
      page->addChild(*p);
    }
    {
      auto* p = desc.defineStringParam(kParamWorkerScript);
      p->setLabels("Worker script", "Worker", "Worker");
      p->setStringType(OFX::eStringTypeFilePath);
      p->setDefault(kDefaultWorker);
      page->addChild(*p);
    }
    {
      auto* p = desc.defineStringParam(kParamSrcRoot);
      p->setLabels("MatAnyone2 src", "Src", "Src");
      p->setStringType(OFX::eStringTypeFilePath);
      p->setDefault(kDefaultSrc);
      p->setHint("Directory containing matanyone2/ package");
      page->addChild(*p);
    }
    {
      auto* p = desc.defineStringParam(kParamDevice);
      p->setLabels("Device", "Device", "Device");
      p->setDefault("auto");
      p->setHint("auto | cpu | cuda | cuda:0");
      page->addChild(*p);
    }
    {
      auto* p = desc.defineBooleanParam(kParamPreferNeural);
      p->setLabels("Prefer Neural", "Neural", "Neural");
      p->setDefault(true);
      p->setHint("Use official MatAnyone2 when model/venv/src are present");
      page->addChild(*p);
    }
    {
      auto* p = desc.defineIntParam(kParamErode);
      p->setLabels("Erode", "Erode", "Erode");
      p->setDefault(10);
      p->setRange(0, 64);
      p->setDisplayRange(0, 32);
      page->addChild(*p);
    }
    {
      auto* p = desc.defineIntParam(kParamDilate);
      p->setLabels("Dilate", "Dilate", "Dilate");
      p->setDefault(10);
      p->setRange(0, 64);
      p->setDisplayRange(0, 32);
      page->addChild(*p);
    }
    {
      auto* p = desc.defineIntParam(kParamWarmup);
      p->setLabels("Warmup frames", "Warmup", "Warmup");
      p->setDefault(10);
      p->setRange(0, 64);
      p->setDisplayRange(0, 32);
      p->setHint("Official n_warmup; first-frame prediction frames before temporal memory");
      page->addChild(*p);
    }
    {
      auto* p = desc.defineIntParam(kParamMaxSize);
      p->setLabels("Max internal size", "MaxSize", "MaxSize");
      p->setDefault(-1);
      p->setRange(-1, 2048);
      p->setDisplayRange(-1, 1024);
      p->setHint("-1 = no downscale; otherwise min(w,h) limit");
      page->addChild(*p);
    }
    {
      auto* p = desc.defineDoubleParam(kParamSoftness);
      p->setLabels("Edge Softness (demo)", "Softness", "Softness");
      p->setDefault(2.0);
      p->setRange(0.0, 16.0);
      p->setDisplayRange(0.0, 8.0);
      page->addChild(*p);
    }
    {
      auto* p = desc.defineDoubleParam(kParamTemporal);
      p->setLabels("Temporal Blend (demo)", "Temporal", "Temporal");
      p->setDefault(0.0);
      p->setRange(0.0, 0.95);
      p->setDisplayRange(0.0, 0.95);
      page->addChild(*p);
    }
    {
      auto* p = desc.defineChoiceParam(kParamOutputMode);
      p->setLabels("Output", "Output", "Output");
      p->appendOption("Source + Matte Alpha");
      p->appendOption("Matte as RGB");
      p->appendOption("Foreground Premult");
      p->setDefault(0);
      page->addChild(*p);
    }
    {
      auto* p = desc.definePushButtonParam(kParamReset);
      p->setLabels("Reset Sequence", "Reset", "Reset");
      page->addChild(*p);
    }
    {
      auto* p = desc.definePushButtonParam(kParamReload);
      p->setLabels("Reload", "Reload", "Reload");
      page->addChild(*p);
    }
  }

  OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum) override {
    return new MatAnyonePlugin(handle);
  }
};

}  // namespace

namespace OFX {
namespace Plugin {
void getPluginIDs(OFX::PluginFactoryArray& ids) {
  static MatAnyonePluginFactory p;
  ids.push_back(&p);
}
}  // namespace Plugin
}  // namespace OFX
