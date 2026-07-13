# NativeWindow 软件渲染崩溃修复设计

## 背景

HarmonyOS 模拟器中 EGL/OpenGL ES 探测失败后，游戏切换到 NativeWindow CPU 软件渲染。
渲染线程随后在 `OH_NativeWindow_NativeWindowRequestBuffer` 或
`OH_NativeWindow_NativeWindowFlushBuffer` 内部触发 SIGSEGV。

现有证据表明软件渲染可以成功提交若干帧，随后才在 ProducerSurface 缓冲队列中崩溃。
当前代码未等待 RequestBuffer 返回的 fence，并且 Surface 生命周期回调可能与渲染线程并发
操作同一个 NativeWindow。相关 NativeWindow 接口由官方文档标记为非线程安全。

## 目标

- 保留模拟器的 CPU 软件渲染降级能力。
- 正确等待 Buffer fence，避免在消费者尚未释放 Buffer 时写入。
- 串行化 NativeWindow 的配置、申请、提交和销毁操作。
- 在 Surface 尺寸变化或销毁时，确保渲染线程已停止。
- 明确 NativeWindow 引用所有权，防止使用已释放对象。
- 不改动游戏玩法、ArkUI HUD 或 OpenGL 正常渲染逻辑。

## 方案

### Buffer 同步

软件渲染每帧执行以下顺序：

1. 在 Surface 互斥锁保护下调用 RequestBuffer。
2. 对返回的有效 fenceFd 使用 `poll()` 等待，处理 EINTR/EAGAIN。
3. fence 就绪后关闭 fenceFd。
4. 映射 NativeBuffer、完成 CPU 绘制并取消映射。
5. 使用 `-1` 作为提交 fence 调用 FlushBuffer。
6. 所有失败路径都成对 AbortBuffer、关闭 fence 并释放 NativeBuffer 引用。

等待采用有限超时。超时或 poll 失败时放弃当前 Buffer，而不是继续写入未就绪内存。

### NativeWindow 串行化

在 `Surface` 中增加互斥锁。以下操作必须持有该锁：

- Buffer geometry、format 和 usage 配置。
- RequestBuffer、AbortBuffer 和 FlushBuffer。
- NativeWindow 引用替换与释放。
- EGL/软件渲染 Surface 初始化和销毁的临界生命周期操作。

锁只覆盖 NativeWindow 和 Buffer 生命周期；游戏逻辑更新不纳入该锁，避免扩大竞争范围。

### 生命周期

- `OnSurfaceCreated` 初始化 Surface 后启动渲染线程。
- `OnSurfaceChanged` 先调用 `Loop::stop()` 并等待线程退出，再更新 geometry，最后重新启动。
- `OnSurfaceDestroyed` 先停止线程，再销毁 Surface。
- 保存 XComponent 提供的 NativeWindow 指针前调用 NativeObjectReference；释放时调用
  NativeObjectUnreference，保持引用成对。
- 重复的 Created/Changed 回调不得重复增加引用或重复初始化同一 Surface。

### 错误处理

- fence 等待失败、映射失败或提交失败均记录具体错误码。
- 单帧错误只丢弃当前帧，不直接终止进程。
- Surface 已失效时，渲染函数立即返回。
- 生命周期切换期间不会并行访问 NativeWindow。

## 测试与验证

将 fence 等待逻辑提取为无平台状态的小函数，以便通过本地单元测试验证：

- `fenceFd < 0` 时立即成功。
- fence 已就绪时成功并关闭 fd。
- 超时时返回失败。
- EINTR 时继续等待剩余时间。

生命周期验证包括：

- 构建 Native C++ 模块，确保 HarmonyOS NDK API 使用正确。
- 检查 Created、Changed、Destroyed 路径均遵守先停线程后修改/销毁 Surface 的顺序。
- 在可用设备上冷启动并持续运行，确认不再产生同路径 cppcrash。

## 非目标

- 不更换图形 API。
- 不将降级渲染迁移到 ArkUI Canvas。
- 不重构 800 余行的 Surface 绘制实现。
- 不提交工作区内与本修复无关的已有改动。
