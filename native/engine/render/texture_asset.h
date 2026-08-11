#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class TextureWrap {
  Clamp,
  Repeat,
};

// 由 ArkTS 原始资源字节初始化的纹理。CPU 像素在 EGL context 重建期间保留，
// GPU 句柄只能在持有 current context 时上传、销毁或绑定。
class TextureAsset {
 public:
  bool tryInitialize(const std::vector<uint8_t>& encoded, TextureWrap wrap);
  bool uploadGpuResource();
  void bind(unsigned int unit) const;
  void destroyGpuResource();
  void abandonGpuResource();
  void clear();

  bool ready() const { return !pixels_.empty() && width_ > 0 && height_ > 0; }
  bool gpuReady() const {
#ifdef OHOS_PLATFORM
    return ready() && handle_ != 0u;
#else
    return ready();
#endif
  }
  int width() const { return width_; }
  int height() const { return height_; }
  TextureWrap wrap() const { return wrap_; }
  std::size_t cpuByteCount() const { return pixels_.size(); }
  unsigned int handle() const { return handle_; }

 private:
  std::vector<uint8_t> pixels_;
  int width_ = 0;
  int height_ = 0;
  TextureWrap wrap_ = TextureWrap::Clamp;
  unsigned int handle_ = 0;
};
