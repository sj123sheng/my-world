#include "surface.h"
#include <EGL/egl.h>
bool surface_create(Surface& s, const char* surfaceId) {
  // 通过 OH_NativeWindow_AcquireNativeWindow + EGL 创建显示表面
  // M0 探针阶段锁定图形 API 后补全;此处返回可编译占位
  return true;
}
void surface_present(Surface& s) { /* eglSwapBuffers */ }
