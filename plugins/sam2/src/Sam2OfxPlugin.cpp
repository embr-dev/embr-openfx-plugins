#include "Sam2Engine.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsProcessing.h"

namespace {

constexpr const char* kPluginName = "EmbrSAM2";
constexpr const char* kPluginGrouping = "Embr";
constexpr const char* kPluginDescription =
    "SAM2 image segmentation (Linux OFX). Point/box prompts to mask. "
    "Uses ONNX Runtime; falls back to demo mask if models are missing.";
constexpr const char* kPluginIdentifier = "jp.embr.ofx.SAM2";
constexpr unsigned int kPluginVersionMajor = 0;
constexpr unsigned int kPluginVersionMinor = 1;

constexpr const char* kParamEncoderPath = "encoderPath";
constexpr const char* kParamDecoderPath = "decoderPath";
constexpr const char* kParamDevice = "device";
constexpr const char* kParamUseBox = "useBox";
constexpr const char* kParamBoxX0 = "boxX0";
constexpr const char* kParamBoxY0 = "boxY0";
constexpr const char* kParamBoxX1 = "boxX1";
constexpr const char* kParamBoxY1 = "boxY1";
constexpr const char* kParamPointX = "pointX";
constexpr const char* kParamPointY = "pointY";
constexpr const char* kParamPointLabel = "pointLabel";
constexpr const char* kParamMaskThreshold = "maskThreshold";
constexpr const char* kParamOutputMode = "outputMode";
constexpr const char* kParamReload = "reloadModels";

// Installer default layout: ~/EmbrSAM2/models/sam2/...
constexpr const char* kDefaultEncoderPath = "~/EmbrSAM2/models/sam2/image_encoder.onnx";
constexpr const char* kDefaultDecoderPath = "~/EmbrSAM2/models/sam2/image_decoder.onnx";

std::string expandUserPath(std::string path) {
  if (path.empty()) return path;

  const char* home = std::getenv("HOME");
  const char* embrRoot = std::getenv("EMBR_SAM2_HOME");
  std::string homeDir = home ? home : "";
  std::string rootDir = embrRoot ? embrRoot : (homeDir.empty() ? std::string() : homeDir + "/EmbrSAM2");

  if (!rootDir.empty()) {
    const std::string marker = "$EMBR_SAM2_HOME";
    if (path.rfind(marker, 0) == 0) {
      path.replace(0, marker.size(), rootDir);
    }
  }
  if (!homeDir.empty() && !path.empty() && path[0] == '~') {
    path.replace(0, 1, homeDir);
  }
  return path;
}

enum OutputModeEnum {
  eOutputMaskAsAlpha = 0,
  eOutputMaskAsRgb,
  eOutputForeground
};

class Sam2Processor : public OFX::ImageProcessor {
 public:
  explicit Sam2Processor(OFX::ImageEffect& effect)
      : OFX::ImageProcessor(effect), _srcImg(nullptr), _mask() {}

  void setSrcImg(OFX::Image* img) { _srcImg = img; }
  void setMask(const std::vector<float>* mask, int width, int height) {
    _mask = mask;
    _maskW = width;
    _maskH = height;
  }
  void setOutputMode(OutputModeEnum mode) { _mode = mode; }

  void multiThreadProcessImages(OfxRectI procWindow) override {
    if (!_srcImg || !_dstImg || !_mask) return;

    const OfxRectI& bounds = _srcImg->getBounds();
    for (int y = procWindow.y1; y < procWindow.y2; ++y) {
      if (_effect.abort()) break;
      for (int x = procWindow.x1; x < procWindow.x2; ++x) {
        float* dst = static_cast<float*>(_dstImg->getPixelAddress(x, y));
        const float* src = static_cast<const float*>(_srcImg->getPixelAddress(x, y));
        if (!dst || !src) continue;

        const int mx = std::clamp(x - bounds.x1, 0, _maskW - 1);
        const int my = std::clamp(y - bounds.y1, 0, _maskH - 1);
        const float m = (*_mask)[static_cast<size_t>(my) * _maskW + mx];

        switch (_mode) {
          case eOutputMaskAsRgb:
            dst[0] = m;
            dst[1] = m;
            dst[2] = m;
            dst[3] = 1.f;
            break;
          case eOutputForeground:
            dst[0] = src[0] * m;
            dst[1] = src[1] * m;
            dst[2] = src[2] * m;
            dst[3] = m;
            break;
          case eOutputMaskAsAlpha:
          default:
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = m;
            break;
        }
      }
    }
  }

 private:
  OFX::Image* _srcImg;
  const std::vector<float>* _mask;
  int _maskW = 0;
  int _maskH = 0;
  OutputModeEnum _mode = eOutputMaskAsAlpha;
};

class Sam2Plugin : public OFX::ImageEffect {
 public:
  explicit Sam2Plugin(OfxImageEffectHandle handle)
      : ImageEffect(handle),
        _dstClip(nullptr),
        _srcClip(nullptr),
        _engineLoaded(false) {
    _dstClip = fetchClip(kOfxImageEffectOutputClipName);
    _srcClip = fetchClip(kOfxImageEffectSimpleSourceClipName);

    _encoderPath = fetchStringParam(kParamEncoderPath);
    _decoderPath = fetchStringParam(kParamDecoderPath);
    _device = fetchChoiceParam(kParamDevice);
    _useBox = fetchBooleanParam(kParamUseBox);
    _boxX0 = fetchDoubleParam(kParamBoxX0);
    _boxY0 = fetchDoubleParam(kParamBoxY0);
    _boxX1 = fetchDoubleParam(kParamBoxX1);
    _boxY1 = fetchDoubleParam(kParamBoxY1);
    _pointX = fetchDoubleParam(kParamPointX);
    _pointY = fetchDoubleParam(kParamPointY);
    _pointLabel = fetchChoiceParam(kParamPointLabel);
    _maskThreshold = fetchDoubleParam(kParamMaskThreshold);
    _outputMode = fetchChoiceParam(kParamOutputMode);
    _reload = fetchPushButtonParam(kParamReload);
  }

  void render(const OFX::RenderArguments& args) override {
    std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
    std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));
    if (!dst.get() || !src.get()) {
      OFX::throwSuiteStatusException(kOfxStatFailed);
    }

    ensureEngineLoaded();

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
          continue;
        }
        o[0] = p[0];
        o[1] = p[1];
        o[2] = p[2];
      }
    }

    embr::Sam2Box box;
    bool useBox = false;
    _useBox->getValueAtTime(args.time, useBox);
    box.enabled = useBox;
    double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    _boxX0->getValueAtTime(args.time, x0);
    _boxY0->getValueAtTime(args.time, y0);
    _boxX1->getValueAtTime(args.time, x1);
    _boxY1->getValueAtTime(args.time, y1);
    // Params are normalized 0..1 of frame for host independence.
    box.x0 = static_cast<float>(x0) * width;
    box.y0 = static_cast<float>(y0) * height;
    box.x1 = static_cast<float>(x1) * width;
    box.y1 = static_cast<float>(y1) * height;

    std::vector<embr::Sam2Point> points;
    double px = 0.5, py = 0.5;
    int labelChoice = 0;
    _pointX->getValueAtTime(args.time, px);
    _pointY->getValueAtTime(args.time, py);
    _pointLabel->getValueAtTime(args.time, labelChoice);
    embr::Sam2Point pt;
    pt.x = static_cast<float>(px) * width;
    pt.y = static_cast<float>(py) * height;
    pt.label = (labelChoice == 0) ? 1 : 0;
    points.push_back(pt);

    std::vector<float> mask;
    _engine.run(rgb.data(), width, height, points, box, mask);
    if (mask.size() != static_cast<size_t>(width) * height) {
      mask.assign(static_cast<size_t>(width) * height, 0.f);
    }

    int modeChoice = 0;
    _outputMode->getValueAtTime(args.time, modeChoice);

    Sam2Processor processor(*this);
    processor.setDstImg(dst.get());
    processor.setSrcImg(src.get());
    processor.setMask(&mask, width, height);
    processor.setOutputMode(static_cast<OutputModeEnum>(modeChoice));
    processor.setRenderWindow(args.renderWindow);
    processor.process();
  }

  bool isIdentity(const OFX::IsIdentityArguments& /*args*/,
                  OFX::Clip*& /*identityClip*/,
                  double& /*identityTime*/) override {
    return false;
  }

  void changedParam(const OFX::InstanceChangedArgs& /*args*/, const std::string& paramName) override {
    if (paramName == kParamReload || paramName == kParamEncoderPath ||
        paramName == kParamDecoderPath || paramName == kParamDevice) {
      _engineLoaded = false;
    }
  }

 private:
  void ensureEngineLoaded() {
    if (_engineLoaded) return;

    embr::Sam2EngineConfig cfg;
    _encoderPath->getValue(cfg.encoderPath);
    _decoderPath->getValue(cfg.decoderPath);
    cfg.encoderPath = expandUserPath(cfg.encoderPath);
    cfg.decoderPath = expandUserPath(cfg.decoderPath);
    int deviceChoice = 0;
    _device->getValue(deviceChoice);
    cfg.device = (deviceChoice == 1) ? "cuda" : (deviceChoice == 2) ? "cpu" : "auto";
    double thr = 0.0;
    _maskThreshold->getValue(thr);
    cfg.maskThreshold = static_cast<float>(thr);
    cfg.threads = 4;

    _engine.load(cfg);
    _engineLoaded = true;
  }

  OFX::Clip* _dstClip;
  OFX::Clip* _srcClip;

  OFX::StringParam* _encoderPath;
  OFX::StringParam* _decoderPath;
  OFX::ChoiceParam* _device;
  OFX::BooleanParam* _useBox;
  OFX::DoubleParam* _boxX0;
  OFX::DoubleParam* _boxY0;
  OFX::DoubleParam* _boxX1;
  OFX::DoubleParam* _boxY1;
  OFX::DoubleParam* _pointX;
  OFX::DoubleParam* _pointY;
  OFX::ChoiceParam* _pointLabel;
  OFX::DoubleParam* _maskThreshold;
  OFX::ChoiceParam* _outputMode;
  OFX::PushButtonParam* _reload;

  embr::Sam2Engine _engine;
  bool _engineLoaded;
};

class Sam2PluginFactory : public OFX::PluginFactoryHelper<Sam2PluginFactory> {
 public:
  Sam2PluginFactory()
      : OFX::PluginFactoryHelper<Sam2PluginFactory>(
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

  void describeInContext(OFX::ImageEffectDescriptor& desc, OFX::ContextEnum /*context*/) override {
    OFX::ClipDescriptor* srcClip =
        desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(false);

    OFX::ClipDescriptor* dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(OFX::ePixelComponentRGBA);
    dstClip->setSupportsTiles(false);

    OFX::PageParamDescriptor* page = desc.definePageParam("Controls");

    {
      OFX::StringParamDescriptor* param = desc.defineStringParam(kParamEncoderPath);
      param->setLabels("Encoder ONNX", "Encoder", "Encoder");
      param->setStringType(OFX::eStringTypeFilePath);
      param->setDefault(kDefaultEncoderPath);
      page->addChild(*param);
    }
    {
      OFX::StringParamDescriptor* param = desc.defineStringParam(kParamDecoderPath);
      param->setLabels("Decoder ONNX", "Decoder", "Decoder");
      param->setStringType(OFX::eStringTypeFilePath);
      param->setDefault(kDefaultDecoderPath);
      page->addChild(*param);
    }
    {
      OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamDevice);
      param->setLabels("Device", "Device", "Device");
      param->appendOption("auto");
      param->appendOption("cuda");
      param->appendOption("cpu");
      param->setDefault(0);
      page->addChild(*param);
    }
    {
      OFX::BooleanParamDescriptor* param = desc.defineBooleanParam(kParamUseBox);
      param->setLabels("Use Box", "Use Box", "Use Box");
      param->setDefault(true);
      page->addChild(*param);
    }
    auto addNorm = [&](const char* name, const char* label, double def) {
      OFX::DoubleParamDescriptor* param = desc.defineDoubleParam(name);
      param->setLabels(label, label, label);
      param->setDefault(def);
      param->setRange(0.0, 1.0);
      param->setDisplayRange(0.0, 1.0);
      page->addChild(*param);
    };
    addNorm(kParamBoxX0, "Box X0", 0.25);
    addNorm(kParamBoxY0, "Box Y0", 0.25);
    addNorm(kParamBoxX1, "Box X1", 0.75);
    addNorm(kParamBoxY1, "Box Y1", 0.75);
    addNorm(kParamPointX, "Point X", 0.5);
    addNorm(kParamPointY, "Point Y", 0.5);
    {
      OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamPointLabel);
      param->setLabels("Point Label", "Point Label", "Point Label");
      param->appendOption("foreground");
      param->appendOption("background");
      param->setDefault(0);
      page->addChild(*param);
    }
    {
      OFX::DoubleParamDescriptor* param = desc.defineDoubleParam(kParamMaskThreshold);
      param->setLabels("Mask Threshold", "Threshold", "Threshold");
      param->setDefault(0.0);
      param->setRange(0.0, 1.0);
      param->setDisplayRange(0.0, 1.0);
      param->setHint("0 keeps soft mask; >0 hard-thresholds");
      page->addChild(*param);
    }
    {
      OFX::ChoiceParamDescriptor* param = desc.defineChoiceParam(kParamOutputMode);
      param->setLabels("Output", "Output", "Output");
      param->appendOption("Source + Mask Alpha");
      param->appendOption("Mask as RGB");
      param->appendOption("Foreground Premult");
      param->setDefault(0);
      page->addChild(*param);
    }
    {
      OFX::PushButtonParamDescriptor* param = desc.definePushButtonParam(kParamReload);
      param->setLabels("Reload Models", "Reload", "Reload");
      page->addChild(*param);
    }
  }

  OFX::ImageEffect* createInstance(OfxImageEffectHandle handle,
                                   OFX::ContextEnum /*context*/) override {
    return new Sam2Plugin(handle);
  }
};

}  // namespace

namespace OFX {
namespace Plugin {
void getPluginIDs(OFX::PluginFactoryArray& ids) {
  static Sam2PluginFactory factory;
  ids.push_back(&factory);
}
}  // namespace Plugin
}  // namespace OFX
