# Task 5 实施报告：固定循环集成输入、移动、相机与锁定

## 实现范围

- `Loop` 集成 `TouchRouter`、`VirtualJoystick`、`CameraGesture`、
  `PlayerIntent`、`PlayerController`、`ThirdPersonCamera` 和 `SoftTargeting`。
- 输入事件按队列顺序路由；`PointerUp/PointerCancel` 在 Router 删除角色映射前
  读取释放前角色，再结束对应控制器。
- 固定更新严格按“消费视角增量、角色、相机、候选、软锁定”的顺序执行；道具候选
  ID 固定由数组索引加一生成。
- `resetInput()` 清空触点角色、摇杆持续移动、相机手势未消费增量、Intent 和输入队列；
  `stop()` 和 Surface 未就绪路径触发全局清理；`PointerCancel` 只释放对应指针角色。
- `GameSnapshot` 新增移动、相机和目标距离诊断字段；`Loop` 写入软锁定结果中的真实
  `targetDist`。桥接层仍由 Task 6 接线，本任务不越界修改 `native_bridge.cpp`。
- 删除旧触点目标移动逻辑及 GL/软件渲染的旧目标环，保留场景、道具、粒子和玩家绘制。
- CMake 注册数学/软锁定 include 目录、角色控制器和软锁定源码。

## 授权范围扩展

Task 5 brief 的集成断言需要 `TouchRouter::activeCount()`，但前置公开接口尚未提供。
经总任务代理明确授权，额外修改：

- `native/engine/input/touch_router.h`：只新增 `activeCount()`。
- `tests/test_touch_controls.cpp`：只新增释放和 `clear()` 后的数量断言。

其余修改保持在 brief 指定文件内。

## TDD 证据

首次 RED 被 macOS 缺失 HarmonyOS `native_window/external_window.h` 提前阻断，退出码
`1`。按 brief 增加平台条件编译后，HarmonyOS 的 `OHOS_PLATFORM` 分支保持原样，
macOS 测试分支仅提供不透明平台类型并跳过平台绘制/日志调用。

随后使用以下命令重新验证 RED：

```bash
c++ -std=c++17 \
  -isystem /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk/usr/include/c++/v1 \
  -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk \
  -I. tests/test_loop_integration.cpp native/engine/core/loop.cpp \
  native/engine/render/camera.cpp \
  native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp \
  -o /tmp/test_loop_integration
```

结果退出码 `1`，按预期报告 `GameSnapshot::moveX/moveY/cameraYaw/targetDist`、
`Loop::intent/camera/resetInput/touchRouter` 不存在；同时旧循环引用已删除的
`Player::targetX/targetY`，证明测试命中新旧链路切换点。

完成最小实现后，以同一源码集合编译并运行，退出码 `0`，无输出。

## 覆盖验证

使用显式 macOS SDK 参数和 `-Wall -Wextra -Werror` 并行编译运行：

- `tests/test_touch_controls.cpp`
- `tests/test_player_controller.cpp` + `player_controller.cpp`
- `tests/test_camera.cpp` + `camera.cpp`
- `tests/test_soft_targeting.cpp` + `soft_targeting.cpp`
- `tests/test_loop_integration.cpp` + 循环、相机、角色控制器、软锁定源码

五组命令均退出码 `0`，无警告、标准输出或错误输出。集成测试覆盖双指移动/视角、
PointerUp 清理、`stop()` 清理、Surface 失效清理、稳定候选 ID 和真实目标距离。

HarmonyOS 生产分支另用 SDK clang 做语法验证：

```bash
/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/clang \
  --target=aarch64-linux-ohos \
  --sysroot=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot \
  -x c++ -std=c++17 -Wall -Wextra -Werror -DOHOS_PLATFORM \
  -I. -Inative -fsyntax-only native/engine/core/loop.cpp \
  native/engine/render/surface.cpp native/engine/render/camera.cpp \
  native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp
```

结果退出码 `0`，证明生产条件编译路径可通过语法和警告检查。

完整 HAP 构建尝试：

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk/default \
  /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw \
  assembleHap --mode module -p module=entry@default -p product=default
```

结果退出码 `255`，环境在进入项目编译前失败：

```text
00303168 Configuration Error
Error Message: SDK component missing.
```

未设置 `DEVECO_SDK_HOME` 时另会报告 `00303217 Invalid value of
'DEVECO_SDK_HOME'`。因此完整 HAP 产物未验证，阻断属于本机 SDK 完整性而非本次源码
编译错误。

## 自审结论与边界

- 正 yaw 的相机前向沿用 Task 2/4 公共约定 `(sin(yaw), cos(yaw))`，正向旋转朝
  `+X`。
- `lookDelta` 每个固定步先复制再清零；同一渲染帧没有固定步时仍保留到下一步，
  有多个固定步时只消费一次。
- 软锁定距离直接来自当前 `TargetSelection::distance`，无目标时归零。
- `surface.props` 当前是 vector；候选 ID 按索引从 1 开始，在数组顺序不变时跨帧稳定。
- `native_bridge.cpp` 当前仍把对外 `targetDist` 写成 `0.0`，由 Task 6 负责将新增快照
  字段接到 NAPI；Task 5 的 Loop/GameSnapshot 已提供真实值。
- 完整 HAP 构建仍需在 SDK 组件完整的 DevEco 环境复验。

---

# Task 5 审查修复报告

## 审查结论

本轮逐项核对并修复全部 Important，同时处理两个 Minor：保留输入控制器配置，以及恢复
Task 5 集成时误删的粒子固定步更新。修复没有改变角色移动、相机、软锁定或输入路由的
决定性结果。

## Important 修复

### 1. 输入队列与新会话隔离

- `InputQueue::clear()` 在一次互斥锁临界区内交换空队列，替代 `resetInput()` 中逐个
  `pop()` 的非原子清空。
- `Loop::start()` 仅在 `LifecycleState::start()` 接受新会话后、启动线程前再次调用
  `resetInput()`。因此停止或 Surface 失效期间排入的旧触摸事件不会在恢复时重放；
  新会话清理后的事件仍可正常入队。
- 回归测试覆盖队列清空后可继续使用，以及启动前旧事件不会移动玩家。

### 2. 同批相机指针切换不丢增量

- `CameraGesture::begin()` 只登记新指针和起始坐标，不再清除尚未消费的
  `accumulated_`。
- 只有 `consumeDelta()` 或全局生命周期清理 `clear()` 才清增量。
- 单元测试与 Loop 顺序测试均覆盖 `old Move -> old Up -> new Down -> consume`。

### 3. `PointerCancel` 按指针释放

- `Loop::processInput()` 不再把任意 `PointerCancel` 升级为全局 `resetInput()`，也不再
  提前返回。
- Cancel 只结束该指针对应的 joystick 或 camera role；另一侧指针保持活动，cancel
  后同批事件继续按队列顺序处理。
- 双指测试覆盖取消相机指针后，移动指针及 cancel 后的移动事件仍生效，同时取消前的
  相机增量仍被消费。
- 全局清理由 `stop()`、Surface 失效和显式 `resetInput()` 负责。

### 4. 目标状态与暂停/渲染停止快照清理

- `resetInput()` 同时清除 `currentTarget`。
- Surface 无效路径在清理后立即发布 renderer-stopped 快照，不再留下上次有效帧。
- `RendererStoppedSnapshot()` 强制把 `targetId` 和 `targetDist` 归零；
  `publishRendererStopped()` 同时清除 Loop 内部目标。
- `stop()` 在输入清理后发布暂停快照，保留当时 Surface 的 ready 状态；只有显式
  renderer-stopped 路径把 `rendererReady` 置为 false。
- 测试覆盖无 fixed tick 的 reset 后快照、普通暂停、显式 renderer-stopped 快照和
  Surface 失效快照。

## Minor 修复

### 5. 清理时保留输入配置

- `VirtualJoystick::clear()` 仅清 pointer/origin/value。
- `CameraGesture::clear()` 仅清 pointer/previous/accumulated。
- `resetInput()` 调用上述 API，不再用默认配置重新构造对象。
- 测试使用非默认 radius 和 sensitivity，验证清理前后行为一致。

### 6. 恢复粒子固定步更新

- 在 `Loop::updateFixed()` 的玩家控制更新后恢复移动尾迹生成、寿命递减和过期回收，
  保留旧实现“生成当步即递减寿命”的时序。
- 将旧函数内跨实例共享的 `static emitTimer` 改为 `Loop::particleEmitTimer`，使多 Loop
  测试和多会话状态互不污染。
- 粒子只读取玩家位置/移动状态并更新渲染列表，不回写移动、相机、目标选择或战斗
  状态，因此不改变决定性玩法结果。
- 测试覆盖生成、寿命递减和回收。

## TDD 证据

新增测试在修复前按预期失败：

- `test_input_queue.cpp`：编译失败，`InputQueue` 无 `clear()`。
- `test_touch_controls.cpp`：编译失败，两个控制器无 `clear()`。
- `test_loop_lifecycle.cpp`：运行断言失败，停止快照保留旧 `targetId`。
- `test_loop_integration.cpp`：运行首先失败于同批新 Camera Down 清掉旧增量。

最小实现完成后，受影响测试转绿；随后执行完整覆盖。

## 完整 7 组覆盖验证

测试统一使用显式 macOS SDK、`/usr/bin/clang++`、C++17 和
`-Wall -Wextra -Werror`：

```bash
SDKROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk
CXX=/usr/bin/clang++
COMMON=(-std=c++17 -Wall -Wextra -Werror \
  -isystem "$SDKROOT/usr/include/c++/v1" -isysroot "$SDKROOT" -I.)

$CXX $COMMON tests/test_input_queue.cpp -o /tmp/task5_test_input_queue
$CXX $COMMON tests/test_touch_controls.cpp -o /tmp/task5_test_touch_controls
$CXX $COMMON tests/test_player_controller.cpp \
  native/gameplay/player/player_controller.cpp -o /tmp/task5_test_player_controller
$CXX $COMMON tests/test_camera.cpp native/engine/render/camera.cpp \
  -o /tmp/task5_test_camera
$CXX $COMMON tests/test_soft_targeting.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o /tmp/task5_test_soft_targeting
$CXX $COMMON tests/test_loop_lifecycle.cpp -o /tmp/task5_test_loop_lifecycle
$CXX $COMMON tests/test_loop_integration.cpp native/engine/core/loop.cpp \
  native/engine/render/camera.cpp native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o /tmp/task5_test_loop_integration
```

以上 7 个二进制均运行退出码 `0`，编译无警告。

## HarmonyOS 生产路径验证

```bash
OHOS_SDK=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native
"$OHOS_SDK/llvm/bin/clang" \
  --target=aarch64-linux-ohos \
  --sysroot="$OHOS_SDK/sysroot" \
  -x c++ -std=c++17 -Wall -Wextra -Werror -DOHOS_PLATFORM \
  -I. -Inative -fsyntax-only \
  native/engine/core/loop.cpp native/engine/render/surface.cpp \
  native/engine/render/camera.cpp \
  native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp
```

结果退出码 `0`，无警告或错误。

## 剩余环境疑虑

本轮按要求验证了所有相关宿主测试和 HarmonyOS 生产条件编译路径。完整 HAP 构建仍受
前一轮报告记录的本机 DevEco SDK component missing 阻断，本轮没有证据表明该环境问题
已经消失，也未把它误报为源码失败。

---

# Task 5 暂停快照与会话粒子状态补充修复

## 修复内容

### 普通暂停与 renderer-stopped 分离

- `Loop::stop()` 不再调用 `publishRendererStopped()`。
- 普通暂停在 `resetInput()` 后发布暂停快照：`moving=false`、`moveX/moveY=0`、
  `targetId/targetDist=0`，并令 `rendererReady` 与 `surface.ready` 一致。
- `resetInput()` 同步把 `surface.player.moving` 置为 false，保证内部玩家状态与暂停快照
  一致。
- 只有 Surface 无效、销毁或初始化失败等 renderer-stopped 路径显式调用
  `publishRendererStopped()`，由其把 `rendererReady` 置为 false。
- 回归测试先建立有效 Surface、活动移动输入和有效目标，再调用 `stop()`，确认输入和
  目标诊断归零且 `rendererReady` 仍为 true；既有 Surface invalid 测试继续确认其为
  false。

### 粒子发射计时器会话隔离

- `resetInput()` 同时把 `particleEmitTimer` 归零。
- `Loop::start()` 接受新会话时已有显式 `resetInput()`，因此新会话不会继承上次暂停或
  Surface 失效前不足一个发射周期的累计时间。
- 回归测试先空闲累计 40ms，执行 reset，再移动 20ms；未生成粒子，证明两个会话的
  累计时间没有合并。

### 报告一致性

- 修正本报告顶部旧表述：`PointerCancel` 只释放对应指针角色，不触发全局清理。
- 修正上一轮停止快照章节，明确普通暂停保留 Surface ready，renderer-stopped 才置
  false。

## TDD 证据

- 修复前，暂停测试失败于 `paused.rendererReady`：当前 `stop()` 错误调用
  `publishRendererStopped()`。
- 仅修复暂停语义后，测试继续失败于粒子列表非空：reset 前 40ms 与 reset 后 20ms
  被错误累加。
- 将 `particleEmitTimer` 纳入 `resetInput()` 后，完整集成测试通过。

## 验证结果

使用 `/usr/bin/clang++`、显式 macOS 15.4 SDK、C++17、
`-Wall -Wextra -Werror` 重新编译并运行：

- `test_input_queue`
- `test_touch_controls`
- `test_player_controller`
- `test_camera`
- `test_soft_targeting`
- `test_loop_lifecycle`
- `test_loop_integration`

7 组测试全部退出码 `0`。

使用 DevEco OpenHarmony SDK clang 对 `loop.cpp`、`surface.cpp`、`camera.cpp`、
`player_controller.cpp` 和 `soft_targeting.cpp` 执行 aarch64 OHOS
`-fsyntax-only -Wall -Wextra -Werror`，退出码 `0`。

## 剩余疑虑

完整 HAP 构建仍受此前记录的本机 DevEco `SDK component missing` 环境问题阻断；本轮
宿主行为测试和 OHOS 生产 C++ 条件编译路径均已验证。

---

# Task 5 Renderer-Stopped 输入诊断清理补充修复

## 修复内容

`RendererStoppedSnapshot()` 除既有的 `rendererReady=false`、`targetId=0` 和
`targetDist=0` 外，现在统一设置：

- `moving=false`
- `moveX=0`
- `moveY=0`

这样 Surface 无效、Surface 销毁/初始化失败后显式调用 `publishRendererStopped()` 的
所有路径都不会向桥接层暴露上一有效帧的活动移动状态或输入向量。普通 `Loop::stop()`
仍使用暂停快照逻辑并保留 `surface.ready`，未重新混用 renderer-stopped 语义。

## 测试覆盖

- `test_loop_lifecycle` 先构造 `moving=true`、非零 `moveX/moveY`、有效目标和
  `rendererReady=true` 的快照，再通过 `RendererStoppedSnapshot()` 转换，断言渲染、
  移动、输入和目标诊断全部归零，同时保留 tick、HP、玩家位置等玩法状态。
- `test_loop_integration` 在显式 `publishRendererStopped()` 前发布活动移动/非零输入/
  有效目标快照，验证全部清零。
- Surface invalid 集成路径先通过真实输入和 fixed tick 发布活动移动、非零输入和有效
  目标快照，再令 `surface.ready=false`，验证 renderer、移动、输入和目标诊断全部清零。

## TDD 证据

修复前，生命周期测试和 Loop 显式发布测试均首先失败于 `stopped.moving` 仍为 true；
给纯转换函数补齐三个字段后，两项受影响测试转绿。

## 完整验证

使用 `/usr/bin/clang++`、显式 macOS 15.4 SDK、C++17 和
`-Wall -Wextra -Werror` 重新编译运行以下 7 组测试，均退出码 `0`：

- `test_input_queue`
- `test_touch_controls`
- `test_player_controller`
- `test_camera`
- `test_soft_targeting`
- `test_loop_lifecycle`
- `test_loop_integration`

DevEco OpenHarmony SDK clang 对 `loop.cpp`、`surface.cpp`、`camera.cpp`、
`player_controller.cpp`、`soft_targeting.cpp` 执行 aarch64 OHOS
`-fsyntax-only -Wall -Wextra -Werror`，退出码 `0`。

## 剩余疑虑

完整 HAP 构建仍受此前记录的本机 DevEco `SDK component missing` 环境问题阻断；本轮
宿主行为与 OHOS C++ 生产条件编译路径已验证。
