# Task 3：走跑迟滞与实际 clip 交叉混合报告

## 实现

- 新增 `LocomotionGait { Unknown, Walk, Run }` 和 `ChooseLocomotionGait`：初始阈值为
  `0.35`，Walk 仅在 `>0.40` 进入 Run，Run 仅在 `<0.30` 回到 Walk；非有限输入回退
  `0`。
- `ResolveClip` 新增可选 gait 参数。Walk 候选严格为 `walk`、`Walking_B`、`run`；旧调用
  仍用 Unknown 的初始判定保持兼容。`Walking_B` 被归类为循环 clip。
- `SkinnedAnimationState` 独立保存 gait，`Run` 时更新、`Idle` 与 `reset()` 时清空。
- 新增按实际 clip 名的四参数 `AnimationBlendSeconds`：仅同属 `Run` 且 clip 变化时混合
  `0.15s`；其他切换继续调用原有两参数分类函数，保留主动动作缺 clip 的
  `0.12/0.20/0.25s` 语义。

## RED → GREEN

1. gait 测试先编译失败：`LocomotionGait` / `ChooseLocomotionGait` 未定义（18 个预期错误）。
2. 实现 gait API 后，动画测试源码通过；简报的链接源集合漏掉当前基线已有的世界依赖，补充
   `environment_collision.cpp` 与 `terrain_heightfield.cpp` 后通过。
3. walk→run 混合测试在旧路径失败于首帧姿态相等断言，证明原逻辑在同一 `Run` 意图内硬切。
4. 实现实例 gait 与实际 clip 混合后，两项测试均退出 0。

## 最终测试

```bash
TEST_BIN_DIR=$(mktemp -d /tmp/myworld-animation-final.XXXXXX)
SDKROOT="$(xcrun --show-sdk-path)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" \
  -I. -Inative -Inative/engine/math)
GAMEPLAY_SOURCES=($(rg --files native/gameplay | rg '\\.cpp$'))
HOST_SAFE_SOURCES=(native/engine/world/environment_collision.cpp \
  native/engine/world/terrain_heightfield.cpp)
clang++ "${COMMON[@]}" tests/test_render_animation.cpp \
  native/engine/render/skinned_model.cpp native/engine/render/asset_profile.cpp \
  native/engine/render/environment.cpp native/engine/render/texture.cpp \
  "${GAMEPLAY_SOURCES[@]}" "${HOST_SAFE_SOURCES[@]}" \
  -o "$TEST_BIN_DIR/render_animation"
"$TEST_BIN_DIR/render_animation"
clang++ "${COMMON[@]}" tests/test_skinned_model.cpp \
  native/engine/render/skinned_model.cpp native/engine/render/texture.cpp \
  -o "$TEST_BIN_DIR/skinned_model"
"$TEST_BIN_DIR/skinned_model"
```

结果：两个测试程序均退出 `0`。

## 文件

- `native/engine/render/render_animation.h`
- `native/engine/render/skinned_model.h`
- `native/engine/render/skinned_model.cpp`
- `tests/test_render_animation.cpp`
- `tests/test_skinned_model.cpp`

## 自审与顾虑

- 已确认每实例 gait 不共享，实际 clip 不变不会重置播放时间或进入混合。
- 已确认 Walk 主语义 clip 优先、`Walking_B` 兼容且循环、两者缺失回退 `run`。
- 未新增动画资产；未修改 `PROJECT_STATE.md` / `DECISIONS.md` / `TASKS.md`，因为其中已记录
  本任务的长期决策，重复更新没有新增价值。
- 顾虑：简报的 `render_animation` 原始聚焦链接命令在当前基线缺少既有 world 实现；最终验证已
  用两个 host-safe 静态实现源补齐，未将该基线链接缺口误判为功能失败。
