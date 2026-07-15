# 阶段 3 Task 8 报告：全量验证、真机验收与文档收口

## 结论

- 基线：`639a656`；工作区开始时仅有用户已有的 `build-profile.json5` 修改。
- 自动化：现有 C++ 30/30、Node 1/1、`git diff --check` 全部通过。
- 生产构建：`BUILD SUCCESSFUL`，signed HAP 存在，构建前后
  `build-profile.json5` 哈希一致。
- 真机：BLOCKED。目标设备由 `USB Offline` 变为 hdc 目标列表为空，无法确认连接/解锁；
  未安装、未注入坐标、未采集 UITest dump/截图/HUD 数值，九项均保持未验收。

验证日期：2026-07-16；工作目录：`/Users/xiling/Documents/project/game/my-world`。

## 全量自动化

公共编译参数与执行命令：

```zsh
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
CXX="$(xcrun --find clang++)"
CPP_DEPS=(
  native/engine/core/loop.cpp native/engine/render/camera.cpp
  native/gameplay/player/player_controller.cpp
  native/gameplay/targeting/soft_targeting.cpp
  native/engine/resource/loader.cpp native/engine/resource/save.cpp
  native/platform/harmony/fence_wait.cpp
  native/gameplay/combat/action_state_machine.cpp
  native/gameplay/combat/combat_controller.cpp
  native/gameplay/combat/combat_resources.cpp
  native/gameplay/combat/damage_resolver.cpp
  native/gameplay/combat/resonance.cpp native/gameplay/combat/source_aura.cpp
  native/gameplay/combat/source_reaction_system.cpp
  native/gameplay/combat/training_pulse.cpp
  native/gameplay/combat/training_target.cpp
)
rm -rf /tmp/my-world-task8-tests
mkdir -p /tmp/my-world-task8-tests
for src in tests/test_*.cpp; do
  name="${src:t:r}"
  "$CXX" -std=c++17 -pthread -isysroot "$SDKROOT" \
    -I"$SDKROOT/usr/include/c++/v1" -I. -Inative \
    "$src" "${CPP_DEPS[@]}" -o "/tmp/my-world-task8-tests/$name"
  "/tmp/my-world-task8-tests/$name"
done
node tests/test_bridge_contract.mjs
git diff --check
```

结果：仓库实际共有 30 个 `tests/test_*.cpp`，30/30 均编译、运行退出码 0；Node 1/1
退出码 0；diff-check 退出码 0 且无输出。通过项：

```text
test_action_state_machine            test_camera_render_transform
test_camera                          test_changed_pointer_forwarding
test_combat_config                   test_combat_controller
test_combat_resources                test_config_schema
test_damage_resolver                 test_decision_log
test_event_order                     test_event_queue
test_fence_wait                      test_fixed_step
test_input_queue                     test_loop_integration
test_loop_lifecycle                  test_player_controller
test_pointer_input                   test_resonance_window
test_resonance                       test_resource_manifest
test_save_atomic                     test_snapshot_store
test_soft_targeting                  test_source_abilities
test_source_aura                     test_source_reaction_system
test_touch_controls                  test_training_pulse
```

首次尝试错误地使用了通用
`/Library/Developer/CommandLineTools/usr/include/c++/v1`，首项报 `cstdint file not found`；
改用任务要求的当前 `$SDKROOT/usr/include/c++/v1` 后清空输出目录并全量重跑通过。该首次失败是
验证命令头文件路径错误，不是源码测试失败。

## 生产构建

```zsh
shasum build-profile.json5
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk \
  node /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw.js \
  --mode module -p module=entry@default -p product=default \
  -p requiredDeviceType=phone assembleHap --analyze=normal \
  --parallel --incremental --daemon
shasum build-profile.json5
ls -l entry/build/default/outputs/default/entry-default-signed.hap
shasum entry/build/default/outputs/default/entry-default-signed.hap
```

结果：

```text
BUILD SUCCESSFUL in 2 s 63 ms
build-profile before: a482c4c748ac73c906b8d1b726e0075b000880eb
build-profile after:  a482c4c748ac73c906b8d1b726e0075b000880eb
signed HAP size:      3,885,608 bytes
signed HAP SHA-1:     27e50f5df1ab525117ef7c717332eded7120e1b9
```

## 设备探测与真机验收

hdc：`/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc`。

```zsh
hdc list targets -v
# 2MN0224C12000754 USB Offline localhost
hdc shell bm dump -n com.ethelandev.myworld
# [Fail]ExecuteCommand need connect-key? please confirm a device by help info
hdc kill
hdc start
hdc list targets -v
# [Empty]
```

无法确认设备连接与解锁，故未执行 HAP 安装或 Ability 启动，也未在未确认布局时注入临时按钮
坐标。没有本次真机截图、UITest dump 或 HUD 数值。

```text
[ ] 普攻得到 1→2→3→4，移动、受击、超时重置
[ ] 闪避耗尽体力后拒绝，停止消耗后恢复至 100
[ ] 普通闪避免疫；精准闪避授予 5 秒洞察
[ ] 三源技能分别生效并遵守 3/4/5 秒冷却
[ ] 折光、凝滞、崩解的生命/韧性与状态正确
[ ] 8 秒内三源充满共鸣，终结技消费 100
[ ] 假人破韧、易伤、死亡和 2 秒复位正确
[ ] 移动、相机、战斗按钮并发且无输入粘连
[ ] 停止重启后无冷却、无敌、附着或脉冲残留
```

阻塞解除条件：设备重新连接并解锁后，安装 signed HAP，启动
`com.ethelandev.myworld/EntryAbility`；先截图或 UITest dump 确认六按钮实际位置，再逐项操作并
记录 HUD 数值/截图。阶段 2 的历史真机证据不作为本次移动、相机、战斗并发回归证据。

## 最终审查修复追加证据（2026-07-16）

- 新增 `test_stage3_final_fixes.cpp` 后，fresh C++ 全量为 31/31，Node 1/1，
  `git diff --check` 无输出。
- Hvigor signed HAP 构建为 `BUILD SUCCESSFUL in 12 s 518 ms`，产物 4,081,727 bytes，
  SHA-256 为 `55114bc7196b2a2efbac08092f3d7eb7122e74ab67cfba66819e68941541f60d`。
- `build-profile.json5` 构建前后 SHA-256 均为
  `b1dfc54516dff5408c530d16745175c956df9415d3bb1ce6bd39ee6b06788390`。
- 上述仅更新自动化与构建证据；本报告列出的九项真机出口仍全部未验收、未勾选。

最终复审追加修复后再次 fresh 验证：C++ 31/31、Node 1/1、diff-check clean；Hvigor
`BUILD SUCCESSFUL in 8 s 803 ms`，signed HAP 4,081,855 bytes，SHA-256
`e2358664e37799365188f042c714fcdde9939f0ac14c4fcec2bfa5a3ac7f7f07`。真机状态未变。

epoch 精准闪避最终修复后再次 fresh 验证：C++ 31/31、Node 1/1、diff-check clean；
Hvigor `BUILD SUCCESSFUL in 7 s 475 ms`，signed HAP 4,081,455 bytes，SHA-256
`8f91fe68540c33a99483fd76d5b3fef1f23a78daf8724a7e4665377c26c6db7f`。真机状态未变。

## 文档与提交范围

仅更新 `README.md`、战斗垂直切片路线图和阶段 3 计划。用户已有的
`build-profile.json5` 修改未触碰、未暂存；构建产物和本报告均不纳入提交。
