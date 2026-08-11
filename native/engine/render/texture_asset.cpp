#include "native/engine/render/texture_asset.h"

#include <limits>

#include "native/engine/render/stb_image.h"

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#endif

bool TextureAsset::tryInitialize(const std::vector<uint8_t>& encoded,
                                 TextureWrap wrap) {
  if (encoded.empty() ||
      encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return false;
  }
  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* decoded = stbi_load_from_memory(
      encoded.data(), static_cast<int>(encoded.size()), &width, &height,
      &channels, STBI_rgb_alpha);
  if (decoded == nullptr || width <= 0 || height <= 0) {
    if (decoded != nullptr) stbi_image_free(decoded);
    return false;
  }
  const std::size_t byteCount = static_cast<std::size_t>(width) *
                                static_cast<std::size_t>(height) * 4u;
  destroyGpuResource();
  pixels_.assign(decoded, decoded + byteCount);
  stbi_image_free(decoded);
  width_ = width;
  height_ = height;
  wrap_ = wrap;
  return uploadGpuResource();
}

bool TextureAsset::uploadGpuResource() {
  if (!ready()) return false;
#ifdef OHOS_PLATFORM
  destroyGpuResource();
  glGenTextures(1, &handle_);
  if (handle_ == 0) return false;
  glBindTexture(GL_TEXTURE_2D, handle_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, pixels_.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  const GLint wrapping =
      wrap_ == TextureWrap::Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapping);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapping);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);
#endif
  return true;
}

void TextureAsset::bind(unsigned int unit) const {
#ifdef OHOS_PLATFORM
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, handle_);
#else
  (void)unit;
#endif
}

void TextureAsset::destroyGpuResource() {
#ifdef OHOS_PLATFORM
  if (handle_ != 0) glDeleteTextures(1, &handle_);
#endif
  handle_ = 0;
}

void TextureAsset::abandonGpuResource() { handle_ = 0; }

void TextureAsset::clear() {
  destroyGpuResource();
  pixels_.clear();
  width_ = 0;
  height_ = 0;
}
