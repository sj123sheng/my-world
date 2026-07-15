# Task 6 实施报告：多指 ArkTS 桥接与快照契约

## 实现范围

- `GamePage.ets` 的 XComponent 触摸回调遍历 `event.changedTouches`，每根变化指针各调用
  一次 `pushInput`，完整传递 `event.type`、`touch.id`、`windowX` 和 `windowY`。
- ArkTS `Snapshot`、Native `.d.ts`、页面初始对象和页面 `@State` 同步增加 `moveX`、
  `moveY`、`cameraYaw`、`cameraPitch`，轮询时逐字段同步。
- `native_bridge.cpp` 从 Task 5 `GameSnapshot` 导出四个新增字段，并将 `targetDist` 从
  固定 `0.0` 改为真实的 `snapshot.targetDist`。
- 契约测试覆盖阶段 2 轮询字段、Native 导出字段、`changedTouches` 多指转发和
  `pointerId: touch.id` 指针 ID 保留。

## TouchEvent SDK 契约核对

本机 DevEco OpenHarmony SDK 声明位于：

```text
/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/ets/component/common.d.ts
```

其中全局 `TouchEvent` 自 API 7 起明确声明：

```ts
touches: TouchObject[];
changedTouches: TouchObject[];
```

因此本任务直接使用 `event.changedTouches`。它在 UP/CANCEL 中保留当前变化（离开）的
指针，不退化到可能已不含抬起指针的 `event.touches`，也不固定读取 `touches[0]`。

## TDD 证据

先只扩展 `tests/test_bridge_contract.mjs`，运行：

```bash
node tests/test_bridge_contract.mjs
```

RED 结果退出码 `1`，首个预期失败为：

```text
AssertionError [ERR_ASSERTION]: GamePage polling must assign moveX
```

完成最小实现后，以同一命令复验，退出码 `0`、无输出。

## 构建与语法验证

完整 HAP 构建已尝试：

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk/default \
  /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw \
  assembleHap --mode module -p module=entry@default -p product=default
```

结果退出码 `255`，在进入源码编译前被环境阻断：

```text
00303168 Configuration Error
Error Message: SDK component missing.
```

另外尝试复用 CMake/Ninja 的 arm64 Native 对象目标，CMake 可重新生成，但已有工程 include
配置缺少仓库根目录，编译在未修改的 `surface.h` 处报
`native/gameplay/player/player_controller.h file not found`。这不是本任务桥接源码错误。

随后用相同 HarmonyOS SDK clang 并补齐仓库根目录 include 做 Native 生产路径语法验证：

```bash
/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/clang++ \
  --target=aarch64-linux-ohos \
  --sysroot=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot \
  -x c++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter \
  -DOHOS_PLATFORM -I. -Inative \
  -I/Applications/DevEco-Studio.app/Contents/sdk/default/hms/native/sysroot/usr/include \
  -fsyntax-only entry/src/main/cpp/native_bridge.cpp
```

结果退出码 `0`、无输出。

## 剩余疑虑

- Node 契约测试与 Native HarmonyOS 语法检查均通过。
- ArkTS/HAP 完整编译仍因本机 `SDK component missing` 无法执行；需在 SDK 组件完整的
  DevEco 环境复验。
- CMake include 路径缺少仓库根目录是既有工程配置问题，本任务按 brief 未修改 CMake。
