#include "MatAnyoneNeuralBackend.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace embr {
namespace {

constexpr uint32_t kMagic = 0x4D413201u;
constexpr uint32_t kCmdInit = 1;
constexpr uint32_t kCmdReset = 2;
constexpr uint32_t kCmdStep = 3;
constexpr uint32_t kCmdQuit = 4;

bool writeAll(int fd, const void* data, size_t n) {
  const auto* p = static_cast<const uint8_t*>(data);
  size_t off = 0;
  while (off < n) {
    const ssize_t w = ::write(fd, p + off, n - off);
    if (w < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (w == 0) return false;
    off += static_cast<size_t>(w);
  }
  return true;
}

bool readAll(int fd, void* data, size_t n) {
  auto* p = static_cast<uint8_t*>(data);
  size_t off = 0;
  while (off < n) {
    const ssize_t r = ::read(fd, p + off, n - off);
    if (r < 0) {
      if (errno == EINTR) continue;
      return false;
    }
    if (r == 0) return false;
    off += static_cast<size_t>(r);
  }
  return true;
}

bool writeU32(int fd, uint32_t v) { return writeAll(fd, &v, sizeof(v)); }
bool writeI32(int fd, int32_t v) { return writeAll(fd, &v, sizeof(v)); }
bool readU32(int fd, uint32_t& v) { return readAll(fd, &v, sizeof(v)); }
bool readI32(int fd, int32_t& v) { return readAll(fd, &v, sizeof(v)); }

bool writeString(int fd, const std::string& s) {
  const uint32_t n = static_cast<uint32_t>(s.size());
  return writeU32(fd, n) && (n == 0 || writeAll(fd, s.data(), n));
}

}  // namespace

struct MatAnyoneNeuralBackend::Impl {
  pid_t pid = -1;
  int toWorker = -1;
  int fromWorker = -1;
  std::string error;
  std::mutex mutex;
};

MatAnyoneNeuralBackend::MatAnyoneNeuralBackend() : impl_(std::make_unique<Impl>()) {}

MatAnyoneNeuralBackend::~MatAnyoneNeuralBackend() { stop(); }

bool MatAnyoneNeuralBackend::running() const { return impl_ && impl_->pid > 0; }

const std::string& MatAnyoneNeuralBackend::lastError() const { return impl_->error; }

void MatAnyoneNeuralBackend::stop() {
  if (!impl_) return;
  if (impl_->toWorker >= 0) {
    writeU32(impl_->toWorker, kMagic);
    writeU32(impl_->toWorker, kCmdQuit);
    ::close(impl_->toWorker);
    impl_->toWorker = -1;
  }
  if (impl_->fromWorker >= 0) {
    ::close(impl_->fromWorker);
    impl_->fromWorker = -1;
  }
  if (impl_->pid > 0) {
    int status = 0;
    ::waitpid(impl_->pid, &status, 0);
    impl_->pid = -1;
  }
}

bool MatAnyoneNeuralBackend::start(const MatAnyoneNeuralConfig& config) {
  stop();
  impl_->error.clear();

  if (config.pythonExe.empty() || config.workerScript.empty() || config.modelPath.empty()) {
    impl_->error = "pythonExe/workerScript/modelPath required";
    return false;
  }

  int inPipe[2];
  int outPipe[2];
  if (::pipe(inPipe) != 0 || ::pipe(outPipe) != 0) {
    impl_->error = "pipe failed";
    return false;
  }

  const pid_t pid = ::fork();
  if (pid < 0) {
    impl_->error = "fork failed";
    return false;
  }

  if (pid == 0) {
    ::dup2(inPipe[0], STDIN_FILENO);
    ::dup2(outPipe[1], STDOUT_FILENO);
    ::close(inPipe[0]);
    ::close(inPipe[1]);
    ::close(outPipe[0]);
    ::close(outPipe[1]);
    if (!config.srcRoot.empty()) {
      ::setenv("EMBR_MATANYONE2_SRC", config.srcRoot.c_str(), 1);
      std::string pp = config.srcRoot;
      const char* old = ::getenv("PYTHONPATH");
      if (old && *old) {
        pp.push_back(':');
        pp += old;
      }
      ::setenv("PYTHONPATH", pp.c_str(), 1);
    }
    ::execl(config.pythonExe.c_str(),
            config.pythonExe.c_str(),
            config.workerScript.c_str(),
            static_cast<char*>(nullptr));
    ::_exit(127);
  }

  ::close(inPipe[0]);
  ::close(outPipe[1]);
  impl_->pid = pid;
  impl_->toWorker = inPipe[1];
  impl_->fromWorker = outPipe[0];

  if (!writeU32(impl_->toWorker, kMagic) || !writeU32(impl_->toWorker, kCmdInit) ||
      !writeString(impl_->toWorker, config.modelPath) ||
      !writeString(impl_->toWorker, config.device) || !writeI32(impl_->toWorker, config.erode) ||
      !writeI32(impl_->toWorker, config.dilate) || !writeI32(impl_->toWorker, config.warmup) ||
      !writeI32(impl_->toWorker, config.maxSize)) {
    impl_->error = "failed to write INIT";
    stop();
    return false;
  }

  uint32_t status = 1;
  if (!readU32(impl_->fromWorker, status)) {
    impl_->error = "failed to read INIT response (worker died?)";
    stop();
    return false;
  }
  if (status != 0) {
    uint32_t n = 0;
    readU32(impl_->fromWorker, n);
    std::string msg(n, '\0');
    if (n) readAll(impl_->fromWorker, msg.data(), n);
    impl_->error = msg.empty() ? "INIT failed" : msg;
    stop();
    return false;
  }

  std::cerr << "[embr-matanyone2] neural worker started pid=" << pid
            << " device=" << config.device << std::endl;
  return true;
}

bool MatAnyoneNeuralBackend::reset() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!running()) {
    impl_->error = "worker not running";
    return false;
  }
  if (!writeU32(impl_->toWorker, kMagic) || !writeU32(impl_->toWorker, kCmdReset)) {
    impl_->error = "reset write failed";
    return false;
  }
  uint32_t status = 1;
  if (!readU32(impl_->fromWorker, status)) {
    impl_->error = "reset read failed";
    return false;
  }
  if (status != 0) {
    uint32_t n = 0;
    readU32(impl_->fromWorker, n);
    std::string msg(n, '\0');
    if (n) readAll(impl_->fromWorker, msg.data(), n);
    impl_->error = msg;
    return false;
  }
  return true;
}

bool MatAnyoneNeuralBackend::step(const float* rgbInterleaved,
                                  int width,
                                  int height,
                                  const float* maskIn,
                                  bool hasMask,
                                  std::vector<float>& alphaOut) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!running()) {
    impl_->error = "worker not running";
    return false;
  }
  if (!rgbInterleaved || width <= 0 || height <= 0) {
    impl_->error = "bad image";
    return false;
  }

  const uint32_t pix = static_cast<uint32_t>(width) * static_cast<uint32_t>(height);
  if (!writeU32(impl_->toWorker, kMagic) || !writeU32(impl_->toWorker, kCmdStep) ||
      !writeI32(impl_->toWorker, width) || !writeI32(impl_->toWorker, height) ||
      !writeI32(impl_->toWorker, hasMask ? 1 : 0) ||
      !writeAll(impl_->toWorker, rgbInterleaved, sizeof(float) * pix * 3)) {
    impl_->error = "step write failed";
    return false;
  }
  if (hasMask) {
    if (!maskIn || !writeAll(impl_->toWorker, maskIn, sizeof(float) * pix)) {
      impl_->error = "step mask write failed";
      return false;
    }
  }

  uint32_t status = 1;
  if (!readU32(impl_->fromWorker, status)) {
    impl_->error = "step read status failed";
    return false;
  }
  if (status != 0) {
    uint32_t n = 0;
    readU32(impl_->fromWorker, n);
    std::string msg(n, '\0');
    if (n) readAll(impl_->fromWorker, msg.data(), n);
    impl_->error = msg.empty() ? "step failed" : msg;
    return false;
  }

  int32_t ow = 0, oh = 0;
  if (!readI32(impl_->fromWorker, ow) || !readI32(impl_->fromWorker, oh) || ow <= 0 || oh <= 0) {
    impl_->error = "bad step geometry";
    return false;
  }
  const size_t outN = static_cast<size_t>(ow) * static_cast<size_t>(oh);
  alphaOut.resize(outN);
  if (!readAll(impl_->fromWorker, alphaOut.data(), sizeof(float) * outN)) {
    impl_->error = "alpha read failed";
    return false;
  }

  if (ow != width || oh != height) {
    std::vector<float> resized(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
      const float sy =
          (height > 1) ? static_cast<float>(y) * (oh - 1) / static_cast<float>(height - 1) : 0.f;
      const int y0 = static_cast<int>(sy);
      const int y1 = std::min(y0 + 1, oh - 1);
      const float fy = sy - y0;
      for (int x = 0; x < width; ++x) {
        const float sx =
            (width > 1) ? static_cast<float>(x) * (ow - 1) / static_cast<float>(width - 1) : 0.f;
        const int x0 = static_cast<int>(sx);
        const int x1 = std::min(x0 + 1, ow - 1);
        const float fx = sx - x0;
        const float v00 = alphaOut[static_cast<size_t>(y0) * ow + x0];
        const float v10 = alphaOut[static_cast<size_t>(y0) * ow + x1];
        const float v01 = alphaOut[static_cast<size_t>(y1) * ow + x0];
        const float v11 = alphaOut[static_cast<size_t>(y1) * ow + x1];
        const float v0 = v00 * (1.f - fx) + v10 * fx;
        const float v1 = v01 * (1.f - fx) + v11 * fx;
        resized[static_cast<size_t>(y) * width + x] = v0 * (1.f - fy) + v1 * fy;
      }
    }
    alphaOut.swap(resized);
  }

  impl_->error.clear();
  return true;
}

}  // namespace embr
