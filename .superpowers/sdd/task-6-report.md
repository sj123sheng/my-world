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

## 审查修复补充

### 输入与快照契约

- `pointerId` 已在 `Bridge.ets` 的 `InputEvent` 和 `Index.d.ts` 的 `pushInput` 参数中改为
  必填字段。
- `NativePushInput` 以 `required=true` 读取 `pointerId`，缺失字段会抛出类型错误，不再将
  缺失指针 ID 当作 `0`。
- 契约测试现在读取 `Index.d.ts`，并逐项比较 Bridge `Snapshot`、`.d.ts` 返回对象和
  `NativePullSnapshot` 导出的字段及顺序。
- 对 `moveX`、`moveY`、`cameraYaw`、`cameraPitch`、`targetDist`，测试分别验证
  `napi_create_double` 读取同名 `snapshot` 字段，且 `napi_set_named_property` 使用对应的
  同一 value。
- 多指测试限定在 `changedTouches.forEach` 回调作用域内，验证其中的 `pushInput` 对象确实
  包含 `pointerId: touch.id`。

强化测试后的 RED 结果为退出码 `1`，预期失败信息为：

```text
AssertionError [ERR_ASSERTION]: Bridge InputEvent must require pointerId
```

完成契约修复后重新运行：

```bash
node tests/test_bridge_contract.mjs
```

结果退出码 `0`、无输出。

### CMake 与 OHOS Native 验证

生产目标 `native_game` 已通过 `target_include_directories` 加入 `${NATIVE_ROOT}/..`。重新使用
现有 debug/arm64-v8a CMake/Ninja 对象目标编译 `surface.cpp.o`，CMake 自动重新生成，实际
OHOS clang 命令包含：

```text
-I.../entry/src/main/cpp/../../../../native/..
```

该命令不含额外手工 `-I.`，对象编译退出码 `0`，原先 `surface.h` 中
`native/gameplay/player/player_controller.h file not found` 的错误已消失。

继续使用同一生产目标编译 `native_bridge.cpp.o` 时，也越过上述 include 错误，但在既有
CMake 未指定 C++17 的位置遇到：

```text
error: no template named 'optional' in namespace 'std'
```

随后用同一 OHOS clang、`-std=c++17` 及 CMake 等价 include 列表（仍未使用 `-I.`）对
`native_bridge.cpp` 执行 `-fsyntax-only`，结果退出码 `0`、无输出。

完整 HAP 构建再次尝试后仍在源码编译前被本机环境阻断，退出码 `255`：

```text
00303168 Configuration Error
Error Message: SDK component missing.
```

### 审查修复后的剩余疑虑

- Task 6 Node 契约测试和 OHOS C++17 语法验证通过。
- CMake arm64 生产对象已证明仓库根 include 生效；完整 Native 生产目标仍需后续统一启用
  C++17，当前首个后续错误为 `std::optional` 不可用。
- ArkTS/HAP 完整构建仍需在 SDK 组件完整的 DevEco 环境复验。
