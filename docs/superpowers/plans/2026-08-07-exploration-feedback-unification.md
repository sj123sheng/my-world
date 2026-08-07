# 探索反馈统一化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为地标发现、机关激活、路径门开启和探索奖励建立一次性统一反馈，并贯通 Native 快照、音效、震动与 ArkTS 探索提示。

**Architecture:** Loop 是探索成功事件的唯一生产者，新增轻量 `ExplorationFeedback` 状态保存当前事件，不进入存档。GameSnapshot 只追加事件类型、ID、标题、副标题和剩余时长；Native 负责事件音效，ArkTS 通过快照边沿检测负责统一 Toast 与震动。

**Tech Stack:** HarmonyOS Native C++17、ArkTS/ArkUI、N-API、现有 `AudioBridge`、现有 `Haptics`、Node.js 契约测试、macOS clang++。

## Global Constraints

- `assets/world/world.json` 继续是探索文案和 ID 的唯一事实来源，运行时不解析 JSON。
- 反馈事件只在成功状态变化时产生一次；失败交互不产生探索成功反馈。
- 新快照字段只追加，不改变 V9 存档字段顺序，也不把瞬时反馈写入存档。
- ArkTS 只消费聚合快照，不复制 `ExplorationContent` 状态判断。
- 每个生产行为先写失败测试并观察红灯，再实现最小代码。

---

### Task 1: 纯逻辑反馈状态

**Files:**
- Create: `native/gameplay/world/exploration_feedback.h`
- Create: `native/gameplay/world/exploration_feedback.cpp`
- Create: `tests/test_exploration_feedback.cpp`
- Modify: `entry/src/main/cpp/CMakeLists.txt`

**Interfaces:**
- `enum class ExplorationFeedbackType : uint8_t { None, PoiDiscovered, PuzzleActivated, GateOpened, RewardClaimed };`
- `struct ExplorationFeedback { ExplorationFeedbackType type; int32_t id; std::string title; std::string subtitle; Tick remainingMs; };`
- `class ExplorationFeedbackState { void publish(...); void update(Tick dtMs); const ExplorationFeedback& snapshot() const; };`

- [ ] Write tests for publish values, expiry, and replacement by a newer event.
- [ ] Run `clang++ ... tests/test_exploration_feedback.cpp ...`; confirm failure because the type does not exist.
- [ ] Implement the minimal state object with saturating expiry and default `None`.
- [ ] Re-run the focused test and confirm pass.
- [ ] Add the source to CMake and commit with `feat: 增加探索反馈状态` and the required `Prompt:` body.

### Task 2: Native snapshot and Loop event production

**Files:**
- Modify: `native/engine/core/game_snapshot.h`
- Modify: `native/engine/core/loop.h`
- Modify: `native/engine/core/loop.cpp`
- Modify: `entry/src/main/cpp/native_bridge.cpp`
- Modify: `tests/test_exploration_loop_contract.cpp`

**Interfaces:**
- Snapshot fields: `explorationFeedbackType`, `explorationFeedbackId`, `explorationFeedbackTitle`, `explorationFeedbackSubtitle`, `explorationFeedbackRemainingMs`.
- Loop helper: `publishExplorationFeedback(ExplorationFeedbackType type, int32_t id, const std::string& title, const std::string& subtitle, Tick durationMs)`.

- [ ] Extend the Loop contract test to require feedback publication after successful POI, puzzle, gate and reward state changes, plus snapshot expiry update.
- [ ] Run the test and confirm the missing snapshot/helper fails it.
- [ ] Implement the state member, helper, `ApplyExplorationSnapshot`, and call sites in successful exploration branches; keep existing sound calls intact.
- [ ] Run the focused Loop and bridge tests and confirm pass.
- [ ] Commit with `feat: 发布探索反馈快照` and `Prompt:` body.

### Task 3: Bridge types, GamePage edge detection and unified Toast

**Files:**
- Create: `entry/src/main/ets/ui/ExplorationToast.ets`
- Modify: `entry/src/main/ets/napi/Bridge.ets`
- Modify: `entry/src/main/cpp/types/libnative_game/Index.d.ts`
- Modify: `entry/src/main/ets/pages/GamePage.ets`
- Modify: `tests/test_bridge_contract.mjs`

**Interfaces:**
- `Snapshot` exposes the five feedback fields with matching order and types.
- `ExplorationToast` consumes `feedbackType`, `title`, `subtitle`, `remainingMs`.

- [ ] Add contract assertions for all five fields, Toast mounting, edge detection, and mapping all four non-zero types.
- [ ] Run `node tests/test_bridge_contract.mjs` and confirm red.
- [ ] Add declarations, polling assignments, `prevExplorationFeedbackId` edge detection, and Toast rendering; use existing `Haptics.light()` for POI/puzzle/reward and `Haptics.heavy()` for gate opening.
- [ ] Re-run the Node contract and confirm pass.
- [ ] Commit with `feat: 增加探索反馈提示` and `Prompt:` body.

### Task 4: Verification and project memory

**Files:**
- Modify: `PROJECT_STATE.md`
- Modify: `TASKS.md`
- Modify: `DECISIONS.md`

- [ ] Run focused C++ tests, `node tests/test_bridge_contract.mjs`, `git diff --check`, and the world generator.
- [ ] Run the OHOS `assembleHap` build and confirm `BUILD SUCCESSFUL`.
- [ ] Record the feedback pipeline and remaining real-device testing gaps in project memory.
- [ ] Commit with `feat: 完成探索反馈统一化` and `Prompt:` body.

## Verification Commands

```bash
SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
CLANG="$(xcrun --find clang++)"
COMMON=(-std=c++17 -isysroot "$SDKROOT" -isystem "$SDKROOT/usr/include/c++/v1" -I. -Inative)
"$CLANG" "${COMMON[@]}" tests/test_exploration_feedback.cpp native/gameplay/world/exploration_feedback.cpp -o /tmp/final_exploration_feedback && /tmp/final_exploration_feedback
node tests/test_bridge_contract.mjs
git diff --check
node automation/assets/generate_world_layout.mjs
DEVECO_SDK_HOME=/Applications/DevEco-Studio.app/Contents/sdk /Applications/DevEco-Studio.app/Contents/tools/hvigor/bin/hvigorw assembleHap --mode module -p module=entry@default -p product=default
```
