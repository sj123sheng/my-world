// Surface 资源生命周期中的纯状态决策。
//
// 该头文件不依赖 EGL/GLES，确保宿主机可以验证 late-dirty 消费、替换/清空和
// current-context 销毁门控。GL 资源只允许在 releaseGlResources 为 true 时删除；
// 失败路径由 EGL context 销毁回收驱动对象，CPU 侧仅丢弃已失效的句柄跟踪。

#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

struct PendingModelAsset {
  std::vector<uint8_t> bytes;
  bool dirty = false;

  void replace(std::vector<uint8_t> next) {
    bytes = std::move(next);
    dirty = true;
  }

  bool take(std::vector<uint8_t>& consumed) {
    if (!dirty) return false;
    consumed = bytes;
    dirty = false;
    return true;
  }

  void markDirtyForContextRebuild() { dirty = !bytes.empty(); }

  void clear() {
    bytes.clear();
    dirty = false;
  }
};

enum class SurfaceGlResource {
  SkinnedModels,
  StaticMeshes,
  Shader3D,
  Program2D,
};

inline const std::vector<SurfaceGlResource> kSurfaceGlDestroyOrder = {
    SurfaceGlResource::SkinnedModels,
    SurfaceGlResource::StaticMeshes,
    SurfaceGlResource::Shader3D,
    SurfaceGlResource::Program2D,
};

// Surface 销毁的窄协调接口。Surface 的 EGL/GLES 调用通过这些回调接入，使成功与失败
// 路径共享同一控制流并可在宿主测试中记录实际销毁顺序。
struct SurfaceDestroyOperations {
  std::function<bool()> makeCurrent;
  std::function<void(SurfaceGlResource)> destroyGlResource;
  std::function<void()> abandonGpuResources;
  std::function<void()> unbindCurrent;
  std::function<void()> destroyEglSurface;
  std::function<void()> destroyEglContext;
  std::function<void()> terminateEglDisplay;
};

inline void ExecuteSurfaceDestroy(const SurfaceDestroyOperations& operations) {
  const bool makeCurrentSucceeded = operations.makeCurrent();
  if (makeCurrentSucceeded) {
    for (const SurfaceGlResource resource : kSurfaceGlDestroyOrder) {
      operations.destroyGlResource(resource);
    }
    // 仅解绑刚刚成功绑定到本 Surface 的 context。失败时调用线程原有的 current context
    // 仍归其所有，绝不能在这里解绑。
    operations.unbindCurrent();
  } else {
    // 不能向未知 current context 发出任何 GL 删除；随后的 eglDestroyContext 会
    // 回收该 context 所有 GL 对象，CPU 侧只清除过期句柄。
    operations.abandonGpuResources();
  }
  operations.destroyEglSurface();
  operations.destroyEglContext();
  operations.terminateEglDisplay();
}
