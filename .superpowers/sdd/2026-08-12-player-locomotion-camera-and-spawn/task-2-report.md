# Task 2：用平滑后的真实速度发布移动比例

## 实现

- `PlayerController` 增加只读 `speed()`：返回与控制器实际使用一致的非负有限基础速度；负数和非有限值安全归零。
- 控制器的有效速度改为调用 `speed()`，确保移动控制与发布层共用同一速度钳制规则。
- `Loop::updateFixed` 将本帧疾跑倍率保存为 `playerSpeedScale`，同时传入控制器和动画比例归一化。
- `ActorRenderState.moveRatio` 改为 `clamp(|velocity| / (speed() * playerSpeedScale), 0, 1)`；真实速度或分母非有限、分母不正时发布 `0`。
- 保持 `moving`、`locomotionRateScale = 0.65f`、相机相对方向、移速/疾跑消耗、碰撞和战斗数值不变。

## TDD 证据

### RED

1. 新增 `speed()` 契约测试后按任务命令编译控制器测试，失败：
   `no member named 'speed' in 'PlayerController'`（3 处）。
2. 新增 Loop 集成测试后，从全部 host-safe native 源重建并运行，失败：
   `Assertion failed: (firstRatio > 0.0f && firstRatio < 0.5f)`，退出码 `134`。
   该失败证明旧实现首帧直接发布了摇杆幅度而非平滑速度。

### GREEN / 验证

以下命令均从当前源码重新编译，退出 `0`：

```bash
# Loop：静态链接全部 host-safe native 源（排除 surface、loop 和 Harmony 生命周期源）
clang++ -std=c++17 -pthread -isysroot "$(xcrun --show-sdk-path)" \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" -I. -Inative \
  -Inative/engine/math tests/test_loop_integration.cpp native/engine/core/loop.cpp \
  "${HOST_SOURCES[@]}" -o "$TEST_BIN_DIR/loop"
"$TEST_BIN_DIR/loop"

# 控制器
clang++ -std=c++17 -isysroot "$(xcrun --show-sdk-path)" \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" -I. -Inative \
  tests/test_player_controller.cpp native/gameplay/player/player_controller.cpp \
  -o "$TEST_BIN_DIR/player"
"$TEST_BIN_DIR/player"

# 相机相对移动回归
clang++ -std=c++17 -isysroot "$(xcrun --show-sdk-path)" \
  -isystem "$(xcrun --show-sdk-path)/usr/include/c++/v1" -I. -Inative \
  tests/test_camera_render_transform.cpp \
  native/gameplay/player/player_controller.cpp \
  native/gameplay/targeting/soft_targeting.cpp -o "$TEST_BIN_DIR/camera_render"
"$TEST_BIN_DIR/camera_render"

git diff --check
```

结果：`test_loop_integration: PASS`、`test_player_controller: PASS`、
`test_camera_render_transform: PASS`、`git-diff-check: PASS`。

## 文件

- `native/gameplay/player/player_controller.h`
- `native/gameplay/player/player_controller.cpp`
- `native/engine/core/loop.cpp`
- `tests/test_player_controller.cpp`
- `tests/test_loop_integration.cpp`

## 自审

- `speed()` 的负数、`infinity` 保护由直接测试覆盖；控制器消耗该接口而非保留另一条未钳制的速度路径。
- Loop 的起步、稳态、松手衰减、完全停止比例由真实 `Player::velocity` 集成测试覆盖。
- 归一化分母和控制器 `update()` 使用完全相同的本帧 `playerSpeedScale`；比例有非有限和非正分母保护。
- 相机 yaw=`pi/2` 的既有前向移动回归已通过。

## 顾虑

- 无已知顾虑。验证为宿主 C++ 测试；未进行真机视觉验收。
