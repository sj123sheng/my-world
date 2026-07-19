// texture.cpp: 使用 stb_image 加载 PNG 纹理并上传到 GL。
//
// STB_IMAGE_IMPLEMENTATION 在本文件内定义一次，提供 stb_image 的实现。
// GL 上传在 #ifdef OHOS_PLATFORM 内，非平台侧直接返回 0。

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "native/engine/render/texture.h"

#ifdef OHOS_PLATFORM
#include <GLES3/gl3.h>
#endif

unsigned int loadTexture(const char* path) {
  if (path == nullptr) {
    return 0u;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  // STBI_rgb_alpha 统一输出 RGBA，简化 GL 上传格式
  stbi_uc* pixels = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
  if (pixels == nullptr || width <= 0 || height <= 0) {
    return 0u;
  }

  unsigned int handle = 0u;
#ifdef OHOS_PLATFORM
  glGenTextures(1, &handle);
  glBindTexture(GL_TEXTURE_2D, handle);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
              GL_UNSIGNED_BYTE, pixels);
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, 0);
#endif

  stbi_image_free(pixels);
  return handle;
}
