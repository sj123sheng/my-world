// Surface 资源生命周期中的纯状态决策。
//
// 该头文件不依赖 EGL/GLES，确保宿主机可以验证 late-dirty 消费、替换/清空和
// current-context 销毁门控。GL 资源只允许在 releaseGlResources 为 true 时删除；
// 失败路径由 EGL context 销毁回收驱动对象，CPU 侧仅丢弃已失效的句柄跟踪。

#pragma once

#include <cstdint>
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

struct SurfaceDestroyPlan {
  bool releaseGlResources = false;
  bool discardCpuGlTracking = false;
  bool destroyEglResources = false;
  std::vector<SurfaceGlResource> glDestroyOrder;
};

inline SurfaceDestroyPlan PlanSurfaceDestroy(bool makeCurrentSucceeded) {
  if (!makeCurrentSucceeded) {
    // 不能向未知 current context 发出任何 GL 删除；随后的 eglDestroyContext 会
    // 回收该 context 所有 GL 对象，CPU 侧只清除过期句柄，不能将其误报为已逐项释放。
    return {.releaseGlResources = false,
            .discardCpuGlTracking = true,
            .destroyEglResources = true,
            .glDestroyOrder = {}};
  }
  return {.releaseGlResources = true,
          .discardCpuGlTracking = true,
          .destroyEglResources = true,
          .glDestroyOrder = kSurfaceGlDestroyOrder};
}
