# NativeWindow 软件渲染崩溃修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 HarmonyOS 模拟器软件渲染中 NativeWindow Buffer 同步及 Surface 生命周期竞态导致的 SIGSEGV。

**Architecture:** 将可移植的 fence 等待逻辑放入独立平台工具文件并用本地主机测试覆盖；Surface 内部用同一互斥锁保护 NativeWindow 配置、Buffer 申请/提交及引用释放。XComponent 生命周期回调在修改或销毁 Surface 前同步停止渲染线程。

**Tech Stack:** C++17、HarmonyOS NDK NativeWindow、XComponent、poll、Hvigor/CMake。

## Global Constraints

- 保留模拟器 CPU 软件渲染降级能力。
- 不修改游戏玩法、ArkUI HUD 或 OpenGL 正常绘制逻辑。
- NativeWindow 非线程安全操作必须串行执行。
- RequestBuffer 返回有效 fence 时必须等待完成后才能写 Buffer。
- 超时或 fence 错误仅丢弃当前帧，不继续写入未就绪 Buffer。
- 不提交工作区内与本修复无关的已有改动。

---

### Task 1: 可测试的 fence 等待工具

**Files:**
- Create: `native/platform/harmony/fence_wait.h`
- Create: `native/platform/harmony/fence_wait.cpp`
- Create: `tests/test_fence_wait.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- Consumes: POSIX `poll(int, nfds_t, int)` 和 `close(int)`。
- Produces: `bool waitAndCloseFence(int fenceFd, int timeoutMs)`；函数始终接管非负 fd 的关闭责任。

- [ ] **Step 1: 写失败测试**

```cpp
#include "../native/platform/harmony/fence_wait.h"
#include <cassert>
#include <cerrno>
#include <unistd.h>

int main() {
  assert(waitAndCloseFence(-1, 10));

  int readyPipe[2];
  assert(pipe(readyPipe) == 0);
  const char byte = 'x';
  assert(write(readyPipe[1], &byte, 1) == 1);
  close(readyPipe[1]);
  assert(waitAndCloseFence(readyPipe[0], 50));
  assert(close(readyPipe[0]) == -1 && errno == EBADF);

  int blockedPipe[2];
  assert(pipe(blockedPipe) == 0);
  assert(!waitAndCloseFence(blockedPipe[0], 1));
  assert(close(blockedPipe[0]) == -1 && errno == EBADF);
  close(blockedPipe[1]);
}
```

- [ ] **Step 2: 运行测试确认失败**

Run: `c++ -std=c++17 tests/test_fence_wait.cpp native/platform/harmony/fence_wait.cpp -o /tmp/test_fence_wait && /tmp/test_fence_wait`

Expected: FAIL，提示 `fence_wait.h` 或 `waitAndCloseFence` 不存在。

- [ ] **Step 3: 实现最小 fence 等待函数**

```cpp
// fence_wait.h
#pragma once
bool waitAndCloseFence(int fenceFd, int timeoutMs);

// fence_wait.cpp
#include "fence_wait.h"
#include <cerrno>
#include <poll.h>
#include <unistd.h>

bool waitAndCloseFence(int fenceFd, int timeoutMs) {
  if (fenceFd < 0) return true;
  pollfd descriptor{fenceFd, POLLIN, 0};
  int result;
  do {
    result = poll(&descriptor, 1, timeoutMs);
  } while (result < 0 && (errno == EINTR || errno == EAGAIN));
  const bool ready = result > 0 &&
      (descriptor.revents & (POLLIN | POLLERR | POLLHUP)) != 0;
  close(fenceFd);
  return ready;
}
```

将 `fence_wait.cpp` 加入 `native_game` 的平台源文件列表。

- [ ] **Step 4: 运行测试确认通过**

Run: `c++ -std=c++17 tests/test_fence_wait.cpp native/platform/harmony/fence_wait.cpp -o /tmp/test_fence_wait && /tmp/test_fence_wait`

Expected: PASS，退出码为 0。

- [ ] **Step 5: 提交 Task 1**

```bash
git add native/platform/harmony/fence_wait.h native/platform/harmony/fence_wait.cpp \
  tests/test_fence_wait.cpp entry/src/main/cpp/CMakeLists.txt
git commit -m "fix: 增加NativeWindow fence等待" \
  -m "Prompt: 修复DevEco启动时NativeWindow软件渲染SIGSEGV"
```

### Task 2: NativeWindow 操作与生命周期串行化

**Files:**
- Modify: `native/engine/render/surface.h`
- Modify: `native/engine/render/surface.cpp`
- Modify: `entry/src/main/cpp/native_bridge.cpp`

**Interfaces:**
- Consumes: `bool waitAndCloseFence(int fenceFd, int timeoutMs)`。
- Produces: `bool surface_resize(Surface&, OHNativeWindow*)`；停止渲染线程后安全更新 geometry。
- Produces: `surface_init` 保存 NativeWindow 时增加引用，`surface_destroy` 成对释放引用。

- [ ] **Step 1: 添加生命周期静态回归检查并确认失败**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
surface_h = Path('native/engine/render/surface.h').read_text()
surface_cpp = Path('native/engine/render/surface.cpp').read_text()
bridge = Path('entry/src/main/cpp/native_bridge.cpp').read_text()
assert 'std::mutex windowMutex' in surface_h
assert 'waitAndCloseFence(fenceFd' in surface_cpp
assert 'OH_NativeWindow_NativeObjectReference(window)' in surface_cpp
assert 'OH_NativeWindow_NativeObjectUnreference(s.window)' in surface_cpp
changed = bridge[bridge.index('static void OnSurfaceChanged'):bridge.index('static void OnSurfaceDestroyed')]
assert changed.index('g_loop.stop()') < changed.index('surface_resize(') < changed.index('g_loop.start()')
PY
```

Expected: FAIL，首个断言指出 Surface 尚无互斥锁。

- [ ] **Step 2: 增加 Surface 锁和安全 resize 接口**

在 `Surface` 中增加 `std::mutex windowMutex`，在头文件声明：

```cpp
bool surface_resize(Surface& s, OHNativeWindow* window);
```

`surface_resize` 验证 window 与当前 Surface 相同，在持锁状态下读取并设置 geometry；失败时记录错误并返回 false。

- [ ] **Step 3: 修正 Buffer fence 和失败清理路径**

`softwareDrawFrame` 持有 `windowMutex` 后申请 Buffer，并立即执行：

```cpp
if (!waitAndCloseFence(fenceFd, 3000)) {
  LOGE("Wait buffer fence timed out or failed");
  OH_NativeWindow_NativeWindowAbortBuffer(s.window, windowBuffer);
  return;
}
fenceFd = -1;
```

所有后续失败路径不再重复关闭 fence；成功路径用 `-1` 提交 fence。NativeWindow 的配置、Abort 和 Flush 均在同一把锁内完成。

- [ ] **Step 4: 修正 NativeWindow 引用所有权**

`surface_init` 在保存新 window 前执行：

```cpp
if (OH_NativeWindow_NativeObjectReference(window) != 0) {
  LOGE("Failed to retain native window");
  return false;
}
```

初始化失败或 `surface_destroy` 时使用 `OH_NativeWindow_NativeObjectUnreference` 成对释放；不再对 XComponent 回调传入的对象调用 `OH_NativeWindow_DestroyNativeWindow`。

- [ ] **Step 5: 串行化 XComponent 生命周期回调**

`OnSurfaceChanged` 按以下顺序执行：

```cpp
g_loop.stop();
if (!surface_resize(g_loop.surface, nativeWindow)) {
  LOGE("surface resize failed");
  return;
}
g_loop.start();
```

`OnSurfaceDestroyed` 保持 `stop()` 在 `surface_destroy()` 之前。重复初始化时先停止并销毁旧 Surface，避免泄漏或双重引用。

- [ ] **Step 6: 运行生命周期检查和 fence 测试**

Run: 重复 Step 1 的 Python 检查，然后运行：

`c++ -std=c++17 tests/test_fence_wait.cpp native/platform/harmony/fence_wait.cpp -o /tmp/test_fence_wait && /tmp/test_fence_wait`

Expected: 两项均 PASS，退出码为 0。

- [ ] **Step 7: 执行 HarmonyOS 构建**

Run: `ohpm run build -- --mode module -p module=entry@default -p product=default`

若项目脚本不提供该命令，则使用 DevEco bundled Hvigor：

`hvigorw assembleHap --mode module -p module=entry@default -p product=default`

Expected: Native C++ 与 ArkTS 编译成功，退出码为 0。

- [ ] **Step 8: 提交 Task 2**

```bash
git add native/engine/render/surface.h native/engine/render/surface.cpp \
  entry/src/main/cpp/native_bridge.cpp
git commit -m "fix: 修复NativeWindow并发崩溃" \
  -m "等待Buffer fence并串行化Surface生命周期操作。" \
  -m "Prompt: 修复DevEco启动时NativeWindow软件渲染SIGSEGV"
```

### Task 3: 设备回归验证

**Files:**
- Modify only if needed: `docs/superpowers/specs/2026-07-13-native-window-crash-fix-design.md`

**Interfaces:**
- Consumes: 构建产物 `entry-default-signed.hap` 和 DevEco/HDC 目标设备。
- Produces: 冷启动持续运行记录，不产生 RequestBuffer/FlushBuffer 同路径 cppcrash。

- [ ] **Step 1: 安装并冷启动应用**

Run: 通过 DevEco Studio Run，或在设备可被 hdc 发现时安装构建产物并启动 `com.ethelandev.myworld`。

Expected: 应用进入前台并持续显示软件渲染画面。

- [ ] **Step 2: 执行生命周期回归**

依次执行前后台切换、旋转或窗口尺寸变化，并持续运行至少 60 秒。

Expected: 无 SIGSEGV；日志中没有 ProducerSurface RequestBuffer/FlushBuffer 崩溃栈。

- [ ] **Step 3: 检查工作区范围**

Run: `git status --short && git diff --check`

Expected: 本任务仅涉及计划中列出的文件；用户原有其他未提交文件保持未暂存状态。
