# 阶段 2 最终审查修复报告

## 结论

阶段 2 最终审查的 Critical、sequence 与 Minor findings 已全部修复：相机状态已进入灰盒
渲染，输入只保留 ArkTS 单一生产者，多生产者 sequence 与 FIFO 顺序在线性化临界区内
一致，非正相机距离配置整体回退安全默认。

## 修复内容

1. 新增纯 C++ `CameraRenderState`，提供只读 target/yaw/pitch/distance 与世界到视图变换。
   默认状态为恒等变换；yaw、pitch、distance 分别影响旋转、纵向投影和缩放。
2. `Loop::updateFixed()` 将 `ThirdPersonCamera::renderState()` 复制到 `Surface`。GL 与软件
   渲染的网格、props、particles、player 均调用相同变换和缩放，不修改现有 2D shader。
3. 删除 Native `OnDispatchTouchEvent`、`OH_NativeXComponent_GetTouchEvent` 及其触控入队；
   `DispatchTouchEvent = nullptr`，仅保留 ArkTS `changedTouches` → N-API。
4. `Loop::enqueueInput()` 使用独立 `inputEnqueueMutex`，同一临界区内完成 sequence 分配与
   `InputQueue::push()`；成功入队后才递增 sequence，不递归获取 InputQueue 内部锁。
5. 相机 min/max distance 任一非有限、零、负或倒置时，min/max/default distance 整体回退
   内建默认值。

## TDD 证据

- RED：独立渲染测试因缺少 `camera_render_state.h` 编译失败；Loop 集成测试因 Surface 不含
  `cameraRenderState` 编译失败；零/负距离配置断言失败；Node 契约因 Native 仍含
  `OH_NativeXComponent_GetTouchEvent` 失败。
- sequence RED：临时恢复审查前的 `fetch_add()` 后再运行同一并发测试，稳定触发
  `concurrentEvent.sequence == expectedSequence++` 断言失败（退出码 134）；恢复 mutex
  线性化实现后通过。
- GREEN：新增实现后，定向 `test_camera_render_transform`、`test_camera`、
  `test_loop_integration` 和 `test_bridge_contract.mjs` 全部退出码 0。
- 并发覆盖：8 个 producer 各入队 32 个事件，重复 20 轮；每轮 pop sequence 从 0 开始
  严格单调连续，且总数为 256。

## 完整验证命令与结果

### macOS C++

使用显式 CommandLineTools clang、macOS SDK libc++ 头和参数：

```bash
CLANG="$(xcrun --find clang++)"
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" \
  -isystem "$SDKROOT/usr/include/c++/v1" -stdlib=libc++ \
  -pthread -I. -Inative)
```

逐个编译并运行 `tests/test_*.cpp`，按测试加入所需生产 `.cpp`。

结果：20/20 编译及运行通过，新增项为 `test_camera_render_transform`。

### Node 桥接契约

```bash
node tests/test_bridge_contract.mjs
```

结果：1/1 通过；契约确认 ArkTS `changedTouches` 保留 pointer ID，Native 不含
`GetTouchEvent`/`OnDispatchTouchEvent`，且 `DispatchTouchEvent` 显式为空。

### OHOS arm64 Native

```bash
cmake -S entry/src/main/cpp -B /tmp/my_world_final_fix_ohos_arm64 \
  -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE=/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/build/cmake/ohos.toolchain.cmake \
  -DOHOS_ARCH=arm64-v8a -DOHOS_STL=c++_shared \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build /tmp/my_world_final_fix_ohos_arm64 --target native_game -- -j4
file /tmp/my_world_final_fix_ohos_arm64/libnative_game.so
```

结果：16 个生产编译单元完成并链接通过；产物为 ARM aarch64 ELF 共享库。只有 toolchain
自带未使用参数 warning 和 CMake 兼容性 deprecation warning。

### unsigned HAP

```bash
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
/Applications/DevEco-Studio.app/Contents/tools/node/bin/node \
/Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
--mode module -p module=entry@default -p product=default \
-p requiredDeviceType=phone assembleHap --analyze=normal \
--parallel --incremental --daemon
```

结果：退出码 0，`BUILD SUCCESSFUL in 5 s 710 ms`；生成
`entry/build/default/outputs/default/entry-default-unsigned.hap`。Hvigor 明确提示未配置
`signingConfigs`，跳过签名。

## 提交前最终复验

在空目录 `/tmp/my_world_final_fix_verify_tests` 与
`/tmp/my_world_final_fix_verify_ohos` 从头执行最终矩阵：

- C++：20/20 编译及运行通过。
- Node：1/1 通过。
- OHOS arm64：16 个生产编译单元完整编译并链接，产物仍为 ARM aarch64 ELF。
- HAP：`BUILD SUCCESSFUL in 2 s 53 ms`，unsigned HAP 存在，仍明确跳过签名。
- `git diff --check`：退出码 0。

## 剩余边界

- HAP 未配置签名；unsigned 组装成功不代表可直接安装。
- 本次没有执行真机触控、画面和帧稳定性验收；自动化通过不替代真机出口。

## 最终复审补充修复

最终复审发现首版世界到视图 yaw 矩阵与控制器基向量方向相反。控制器使用
`right=(cos,-sin)`、`forward=(sin,cos)`，因此渲染现在使用严格逆变换：

```text
viewX = cos(yaw) * dx - sin(yaw) * dy
viewY = sin(yaw) * dx + cos(yaw) * dy
```

- 新增 `worldVectorToView()`；玩家世界朝向先经该变换，再用 `atan2` 取得视图朝向，GL 与
  软件路径不再各自推导角度。
- 跨模块测试覆盖 yaw `0`、`0.37`、`1.2`、`-2.1`：controller move 经世界基向量后再
  world-to-view，恢复原 move 方向；SoftTargeting 正前方候选固定落在 view 正前轴。
- 玩家和粒子明确为屏幕空间 billboard：中心位置应用完整相机变换，尺寸只随 distance
  缩放，不受 pitch 拉伸；纯几何测试验证 portrait 视口中 NDC x/y 半径对应相同像素半径。
- props 保持世界几何，显式传递 NDC x/y 半径；GL 与软件 rasterizer 使用同一半径约定，
  非方形 aspect 和 pitch 下不再出现路径尺寸差异。

TDD RED：新增测试最初因缺少 `billboardNdcRadii()`、`worldVectorToView()` 编译失败；补齐
API 后，同一测试通过。

最终复验：

```bash
"$CLANG" "${COMMON[@]}" tests/test_camera_render_transform.cpp \
  native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp \
  -o /tmp/test_camera_render_transform
```

- C++：20/20 编译及运行通过；Node：1/1 通过。
- OHOS arm64：清理构建目录后，16 个生产编译单元完整编译并链接为 ARM aarch64 ELF。
- HAP：最终复验 `BUILD SUCCESSFUL in 8 s 873 ms`，当前用户签名配置生成
  `entry-default-signed.hap`，同时保留 unsigned 产物。
- HAP 构建前后 `git hash-object build-profile.json5` 一致；该用户改动未被修改或暂存。
- `git diff --check`：退出码 0。
