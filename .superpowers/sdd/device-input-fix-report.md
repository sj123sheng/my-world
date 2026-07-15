# 真机输入生产链路修复报告

## 结论

目标真机上的输入根因是：带 `libraryname: 'native_game'` 的 Native XComponent 没有把
触控送达 ArkTS `GamePage.onTouch`。UI Test 单指与 uinput 双指注入均能进入系统，但页面
回调日志一次也没有出现，因此旧的 ArkTS 单一生产者链路无法向游戏循环入队。

当前实现改为 Native XComponent `DispatchTouchEvent` 单一生产者。ArkTS 页面已删除整个
`.onTouch` 和 `pushInput` 调用路径；Native 回调获取 `OH_NativeXComponent_TouchEvent` 后，
每次只使用顶层 changed pointer 的 `type/id/x/y` 映射并调用 `Loop::enqueueInput()`。
`touchPoints/numPoints` 是活动触点快照，不是本次变化点列表，因此不遍历生产动作；未知
顶层动作不入队。

`Bridge.ets` 与 Native 导出的 `pushInput` 继续保留，用于自动化、调试或未来外部输入，
但 `GamePage` 不再调用它，因此当前生产环境没有双重触控生产者。

## TDD 证据

### RED

先扩展 `tests/test_bridge_contract.mjs`，要求：

- `GamePage` 不含 `.onTouch` 和 `pushInput` 生产路径；
- `DispatchTouchEvent` 注册非空 Native 回调；
- 回调调用 `OH_NativeXComponent_GetTouchEvent`；
- 不遍历 `touchEvent.touchPoints` 或按 `numPoints` 分支输入语义；
- 每次只按顶层 changed pointer 的 `type/id/x/y` 映射并入队。

执行：

```bash
node tests/test_bridge_contract.mjs
```

修复前退出码为 `1`，首个失败为：

```text
AssertionError: GamePage must not register an ArkTS touch producer for a library-backed XComponent
```

当时 Native callback 仍为 `DispatchTouchEvent = nullptr`，因此后续 Native 契约同样不满足。

最终审查进一步指出，首版 Native 实现错误地把活动触点快照当成 changed-pointer 列表。
修正前新增第二轮 RED：

- Node 契约失败于 `Native callback must not reinterpret the active-point snapshot as changed pointers`；
- 新纯 C++ 序列测试因 `changed_pointer_forwarder.h` 尚不存在而编译失败。

### GREEN

实现独立 `ForwardChangedPointer` helper，并将 Native callback 收敛为一次顶层字段转发后，
Node 契约和真实双指序列测试均退出码 `0`。没有保留 ArkTS 备用生产路径，临时
`console.info` 也随整个 `.onTouch` 一并删除。

## 实现边界

- `ForwardChangedPointer` 复用既有 `TryMapPointerAction`，只接受 0～3 四种指针动作。
- 序列测试覆盖 id1 Down、id2 Down（活动快照含两指）、id1 Move、id1 Up（只剩 id2）、
  id2 Cancel 和 unknown；每个有效回调恰好生产一条，unknown 不生产。
- `TouchRouter`、`VirtualJoystick` 与 `CameraGesture` 联合验证 id1 Up 后 id2 Camera 职责仍在，
  id2 Cancel 后活动职责清空。
- `Loop::enqueueInput` 保留既有容量拒绝与 sequence 线性化行为。
- `TouchRouter` 保留既有非有限坐标、Surface 尺寸和区域边界校验。
- 没有修改输入语义、相机、渲染或签名配置。

## 完整验证

验证日期：2026-07-15；工作目录：
`/Users/xiling/Documents/project/game/my-world`。

### macOS C++ 与 Node

从空目录 `/tmp/my_world_device_input_tests` 逐项编译并运行仓库全部
`tests/test_*.cpp`，按测试链接对应生产 `.cpp`；编译使用 macOS SDK 的 libc++、
`-std=c++17 -pthread -I. -Inative`。

结果：C++ `21/21` 编译、运行通过；随后
`node tests/test_bridge_contract.mjs` 退出码为 `0`。

### OHOS arm64 Native 全链接

```bash
cmake -S entry/src/main/cpp -B /tmp/my_world_device_input_ohos \
  -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/build/cmake/ohos.toolchain.cmake \
  -DOHOS_ARCH=arm64-v8a -DOHOS_STL=c++_shared \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build /tmp/my_world_device_input_ohos --target native_game -- -j4
file /tmp/my_world_device_input_ohos/libnative_game.so
```

结果：16 个生产编译单元完成并链接，退出码为 `0`；产物为 ARM aarch64 ELF 共享库。
只有 toolchain 自带未使用参数 warning 与 CMake 兼容性 deprecation warning。

### signed HAP

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
--mode module -p module=entry@default -p product=default \
-p requiredDeviceType=phone assembleHap --analyze=normal \
--parallel --incremental --daemon
```

最终审查修正后结果：`BUILD SUCCESSFUL in 8 s 667 ms`，生成
`entry/build/default/outputs/default/entry-default-signed.hap`。用户签名配置
`build-profile.json5` 构建前后 `git hash-object` 均为
`7989eda54360899bd49d6c90ce1f0ca40e364683`，未被本任务修改或暂存。

## 建议的设备复验

连接目标真机后执行：

```bash
hdc list targets
hdc install -r entry/build/default/outputs/default/entry-default-signed.hap
hdc shell hilog -r
hdc shell aa start -a EntryAbility -b com.ethelandev.myworld
hdc shell hilog | grep -E 'Ethelan|OnSurface|EGL|GL_VERSION'
```

再复用已验证稳定的 UI Test 单指与 uinput 双指注入命令，逐项观察：

```text
[ ] 左区单指拖动后 HUD x/y 与 moveX/moveY 变化，角色和画面同步移动
[ ] 右区单指拖动后 cameraYaw/cameraPitch 与画面同步变化
[ ] 左手持续移动时，右手可同时环绕和俯仰
[ ] 任一指针释放不影响另一侧控制
[ ] CANCEL 后对应指针职责释放，不残留移动或镜头增量
[ ] 相机无跳变、翻转或非有限状态
```

建议在复验时保留一段 HiLog 与屏幕录制；如果 HUD 仍不变化，下一步应先确认
`OnDispatchTouchEvent` 是否进入、顶层 changed pointer 的 `type/id/x/y` 是否符合设备实际数据，
不要重新启用 ArkTS `.onTouch` 形成双生产者。

## 剩余疑虑

- 自动化与 signed HAP 构建不能替代真机触控出口，本轮尚未安装并操作设备。
- 契约测试验证源码结构和字段职责；真实设备各类 DOWN/MOVE/UP/CANCEL 回调的顶层字段
  是否与注入动作一致，仍需通过上述复验确认。
