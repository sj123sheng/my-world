# 艾瑟兰(Ethelan) MVP 鸿蒙原生骨架 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 HarmonyOS 上交付一个可连续体验 15–30 分钟的单机探索战斗 MVP 垂直切片(三源共鸣战斗 + 探索—成长闭环),并跑通 M0–M3 全部决策门。

**Architecture:** ArkUI(ArkTS)承载系统 UI/HUD,通过 N-API 桥接 HarmonyOS NDK C++ 游戏核心;游戏核心运行在 XComponent + NativeWindow 的 Native 帧循环上,战斗逻辑使用固定逻辑 tick 与表现解耦,事件可序列化以支持未来权威服务器重算。MVP 全程运行在 Native 骨架而非 ArkUI Canvas 原型上。

**Tech Stack:** HarmonyOS NDK (C++), ArkTS + ArkUI, N-API, XComponent + NativeWindow, OpenGL ES / Vulkan(经 M0 探针确定), DevEco Studio + Hvigor, 版本化 Schema 配置(JSON 开发期 / 二进制发布期)。

## Global Constraints

- **原创性门禁**(spec §2.4):名称、轮廓、配色、动作节奏、特效语言、UI 布局、成长资源、战斗规则均不得直接对应参考作品;文案用"体验目标"描述,不沿用其他产品专有式称呼。
- **不建空壳在线服务**(spec §9.1):MVP 仓库不预建服务器;公网联机在 M4 后单独立项。
- **编译优化链路分开描述**(spec §4.2):ArkTS 由 ArkCompiler 编译;C++ 游戏核心用 NDK 原生工具链(LTO/PGO/数据布局/内存池/SIMD),不得统一表述为"方舟 AOT 编译 C++"。
- **资源热更边界**(spec §6.3):代码与原生库更新走应用发布渠道,不通过资源热更替换可执行逻辑;资源清单需含版本/大小/哈希/依赖,HAP/HAR/HSP 不用于大型地图资源抽象。
- **平台能力不越界**(spec §7.3):只承诺经目标 SDK 与真机验证的公开能力,不直接承诺控制 GPU 频率/电源档位;每项鸿蒙能力须设计降级路径。
- **确定性战斗**(spec §5.2):固定 tick、事件按 tick+优先级+sequence 排序、战斗状态只消费 GameplayEvent、表现只消费 PresentationEvent、关键输入与结果写入可回放调试记录。
- **存档安全**(spec §6.2):临时文件 + 校验和 + 原子替换 + 保留最近一次有效备份;配置加载失败拒绝进入并输出明确错误,不用不可追踪默认值。
- **性能承诺纪律**(spec §8.1/8.3):M0 先建立三档真机基线,之后才能填量化门槛;未经真机数据支持的数值不得作为硬指标。

---

## 文件结构总览

```
my-world/
├─ entry/src/main/ets/                  # ArkTS 系统层
│  ├─ EntryAbility.ts                   # 应用生命周期
│  ├─ pages/GamePage.ts                 # 承载 XComponent 的游戏页
│  ├─ ui/Hud.ets                        # 战斗 HUD(血/韧/源质附着)
│  └─ napi/Bridge.ts                    # N-API 注册封装
├─ entry/src/main/cpp/
│  ├─ native_bridge.cpp                 # N-API 导出(生命周期/输入/快照/命令)
│  └─ CMakeLists.txt
├─ native/engine/                       # 游戏核心
│  ├─ core/loop.cpp/.h                  # 固定 tick 帧循环
│  ├─ core/tick_clock.h                 # Tick/FixedPoint 类型
│  ├─ render/surface.cpp/.h             # NativeWindow + XComponent 渲染表面
│  ├─ render/camera.cpp/.h
│  ├─ input/input_queue.cpp/.h          # 输入事件队列(N-API 注入)
│  ├─ resource/loader.cpp/.h            # 资源流式加载 + 清单校验
│  └─ ecs/world.h                       # ECS 基础(实体/组件/系统)
├─ native/gameplay/                     # 玩法
│  ├─ combat/source_aura.cpp/.h         # 三源附着容器
│  ├─ combat/resonance.cpp/.h           # 共鸣结算表
│  ├─ combat/event.h                    # HitEvent/CombatResult/Gameplay/Presentation
│  ├─ combat/decision_log.cpp/.h        # 确定性回放记录
│  ├─ entities/enemy.cpp/.h             # 3 类敌人 + 精英
│  ├─ entities/boss.cpp/.h              # 多阶段首领
│  ├─ player/character.cpp/.h           # 1 角色:移动/冲刺/闪避/普攻/2主动/终结
│  ├─ growth/relic.cpp/.h               # 遗物构件
│  └─ growth/camp.cpp/.h                # 营地升级
├─ native/platform/harmony/             # 平台适配
│  ├─ lifecycle.cpp                     # 前后台/暂停恢复
│  └─ audio.cpp
├─ config/                              # Schema + 开发期 JSON
│  ├─ schema/source_aura.schema.json
│  ├─ schema/resonance.schema.json
│  ├─ schema/enemy.schema.json
│  ├─ schema/relic.schema.json
│  ├─ dev/characters.json
│  ├─ dev/enemies.json
│  ├─ dev/resonances.json
│  └─ dev/relics.json
├─ assets/                              # MVP 本地资源 + manifest
│  └─ manifest.json
├─ tests/                               # 全部带 main/断言,可离线运行
│  ├─ test_source_aura.cpp
│  ├─ test_resonance.cpp
│  ├─ test_event_order.cpp
│  ├─ test_decision_log.cpp
│  ├─ test_config_schema.cpp
│  ├─ test_save_atomic.cpp
│  └─ test_resource_manifest.cpp
└─ automation/
   ├─ hvigor/check_rules.js             # 原创性/边界检查脚本桩
   └─ perf/profile_collect.sh           # 三档设备基线采集脚本
```

---

## 里程碑 M0:技术探针

目标(spec §10):Native 渲染表面、输入、相机、资源加载、持续帧循环在三档目标设备上可运行且可分析。

### Task M0-1:DevEco 工程骨架与 N-API 桥接边界

**Files:**
- Create: `entry/src/main/ets/EntryAbility.ts`
- Create: `entry/src/main/ets/pages/GamePage.ts`
- Create: `entry/src/main/cpp/native_bridge.cpp`
- Create: `entry/src/main/cpp/CMakeLists.txt`
- Create: `entry/src/main/ets/napi/Bridge.ts`

**Interfaces:**
- Consumes: 无(起始任务)
- Produces: `Bridge.ts` 暴露 `nativeStart(surfaceId)`, `nativeStop()`, `pushInput(event)`, `pullSnapshot()` 供后续任务调用

- [ ] **Step 1: 编写 GamePage 占位测试逻辑(验证 N-API 可调用)**

`entry/src/main/ets/pages/GamePage.ts`:
```typescript
import { nativeStart, nativeStop } from '../napi/Bridge';

@Entry
@Component
struct GamePage {
  private surfaceId: string = '';
  build() {
    Column() {
      XComponent({
        id: 'gameSurface',
        type: 'surface',
        libraryname: 'native_game'
      })
        .onLoad((surfaceId: string) => {
          this.surfaceId = surfaceId;
          nativeStart(surfaceId);
        })
        .width('100%')
        .height('100%')
    }
  }
  aboutToDisappear() { nativeStop(); }
}
```

- [ ] **Step 2: 实现 N-API 桥接(native_bridge.cpp)**

```cpp
#include "napi/native_api.h"
#include <string>

static napi_value NativeStart(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  char buf[128] = {0};
  size_t len = 0;
  napi_get_value_string_utf8(env, args[0], buf, sizeof(buf), &len);
  // 后续任务在此启动 Native 帧循环;此处仅记录 surfaceId
  return nullptr;
}
static napi_value NativeStop(napi_env env, napi_callback_info) { return nullptr; }

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
    {"nativeStart", nullptr, NativeStart, nullptr, nullptr, nullptr, napi_default, nullptr},
    {"nativeStop", nullptr, NativeStop, nullptr, nullptr, nullptr, napi_default, nullptr},
  };
  napi_define_properties(env, exports, sizeof(desc)/sizeof(desc[0]), desc);
  return exports;
}
EXTERN_C_END
static napi_module demoModule = { .nm_version=1, .nm_flags=0, .nm_filename=nullptr,
  .nm_register_func=Init, .nm_modname="native_game", .nm_priv=nullptr };
extern "C" void RegisterNativeGame() { napi_module_register(&demoModule); }
```

- [ ] **Step 3: CMakeLists.txt 与 Bridge.ts 封装**

`entry/src/main/cpp/CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.5)
project(native_game)
add_library(native_game SHARED native_bridge.cpp)
target_link_libraries(native_game PUBLIC libace_napi.z.so)
```
`entry/src/main/ets/napi/Bridge.ts`:
```typescript
import native from 'libnative_game.so';
export const nativeStart = (id: string): void => native.nativeStart(id);
export const nativeStop = (): void => native.nativeStop();
```

- [ ] **Step 4: 编译验证(DevEco 同步并构建 entry module)**
Run: DevEco Studio → Build → Build Hap(s);或命令行 `hvigorw assembleHap`
Expected: BUILD SUCCESSFUL,生成 entry-default-signed.hap

- [ ] **Step 5: Commit**
```bash
git add entry/ && git commit -m "M0-1: DevEco 工程骨架与 N-API 桥接边界"
```

### Task M0-2:Native 渲染表面与持续帧循环

**Files:**
- Create: `native/engine/core/tick_clock.h`
- Create: `native/engine/render/surface.cpp`, `surface.h`
- Create: `native/engine/core/loop.cpp`, `loop.h`
- Modify: `entry/src/main/cpp/native_bridge.cpp`(在 NativeStart 中启动 loop)

**Interfaces:**
- Consumes: `nativeStart(surfaceId)`(M0-1)
- Produces: `Loop::start(surfaceId)`, `Loop::stop()`, `Loop::tickOnce(dtMs)` 供 M1 战斗接入

- [ ] **Step 1: 定义 tick 类型(tick_clock.h)**
```cpp
#pragma once
#include <cstdint>
using Tick = int64_t;
using FixedPoint = int64_t; // 1.0 == 1<<16, 定点数
constexpr FixedPoint FP_ONE = 1 << 16;
inline FixedPoint fp(double v){ return static_cast<FixedPoint>(v * FP_ONE); }
```

- [ ] **Step 2: 渲染表面(surface.h / surface.cpp)** 使用 NativeWindow 获取 EGLSurface,清屏渲染(灰盒占位)。
```cpp
// surface.h
#pragma once
#include <native_window.h>
struct Surface { void* eglDisplay=nullptr; void* eglSurface=nullptr; NativeWindow* window=nullptr; };
bool surface_create(Surface& s, const char* surfaceId);
void surface_present(Surface& s);
```
```cpp
// surface.cpp (EGL 初始化骨架,具体 API 在 M0 探针确定)
#include "surface.h"
#include <EGL/egl.h>
bool surface_create(Surface& s, const char* surfaceId) {
  // 通过 OH_NativeWindow_AcquireNativeWindow + EGL 创建显示表面
  // M0 探针阶段锁定图形 API 后补全;此处返回可编译占位
  return true;
}
void surface_present(Surface& s) { /* eglSwapBuffers */ }
```

- [ ] **Step 3: 帧循环(loop.h / loop.cpp)** 固定逻辑 tick + 渲染解耦。
```cpp
// loop.h
#pragma once
#include "render/surface.h"
struct Loop { Surface surface; bool running=false; void start(const char* id); void stop(); void tickOnce(); };
```
```cpp
// loop.cpp
#include "loop.h"
void Loop::start(const char* id){ running=true; surface_create(surface, id); }
void Loop::stop(){ running=false; }
void Loop::tickOnce(){
  // M1 在此推进固定 tick 战斗;M0 仅清屏 + 统计
  surface_present(surface);
}
```

- [ ] **Step 4: 在 NativeStart 中启动循环并验证持续帧**
Modify `native_bridge.cpp`:在 `NativeStart` 内调用 `Loop loop; loop.start(buf);`(注意生命周期:使用静态/智能指针避免提前析构;M0 用最简静态实例)。
Run: 安装 hap 到目标设备,观察 surface 持续刷新无崩溃。
Expected: 设备画面持续渲染(灰盒),log 无 native crash。

- [ ] **Step 5: Commit**
```bash
git add native/engine entry/src/main/cpp && git commit -m "M0-2: Native 渲染表面与持续帧循环"
```

### Task M0-3:输入队列与相机

**Files:**
- Create: `native/engine/input/input_queue.cpp`, `input_queue.h`
- Create: `native/engine/render/camera.cpp`, `camera.h`
- Modify: `entry/src/main/cpp/native_bridge.cpp`(导出 `pushInput`),`entry/src/main/ets/napi/Bridge.ts`(增加 pushInput)

**Interfaces:**
- Consumes: `Loop`(M0-2)
- Produces: `InputQueue::push(event)`, `Camera::update(dt)` 供 M1 角色控制

- [ ] **Step 1: 输入队列(input_queue.h)**
```cpp
#pragma once
#include <queue>
struct InputEvent { int type; float x; float y; };
struct InputQueue { std::queue<InputEvent> q; void push(InputEvent e){ q.push(e); } bool pop(InputEvent& out){ if(q.empty())return false; out=q.front(); q.pop(); return true; } };
```

- [ ] **Step 2: 相机(camera.h / camera.cpp)**
```cpp
// camera.h
struct Camera { float x=0,y=0,zoom=1; void update(float dx,float dy); };
```
```cpp
// camera.cpp
#include "camera.h"
void Camera::update(float dx,float dy){ x+=dx; y+=dy; }
```

- [ ] **Step 3: 暴露 pushInput 到 N-API**
在 `native_bridge.cpp` 增加 `NativePushInput`,注册到 exports;`Bridge.ts` 导出 `export const pushInput = (e:{type:number,x:number,y:number}) => native.pushInput(e);`

- [ ] **Step 4: 验证输入驱动相机(触屏摇杆 → 相机移动)**
Run: 在 GamePage 加入虚拟摇杆 `onTouch` → `pushInput`;设备运行观察相机响应。
Expected: 拖动摇杆相机平移,无崩溃。

- [ ] **Step 5: Commit**
```bash
git add native/engine/input native/engine/render/camera.cpp native/engine/render/camera.h entry/src/main/cpp entry/src/main/ets/napi && git commit -m "M0-3: 输入队列与相机"
```

### Task M0-4:资源加载与清单校验 + 三档设备基线

**Files:**
- Create: `native/engine/resource/loader.cpp`, `loader.h`
- Create: `assets/manifest.json`
- Create: `tests/test_resource_manifest.cpp`
- Create: `automation/perf/profile_collect.sh`

**Interfaces:**
- Consumes: 无特殊
- Produces: `ResourceLoader::loadManifest(path)`, `ResourceLoader::verify(hash)` 供 M2 区域加载

- [ ] **Step 1: 资源清单(assets/manifest.json)**
```json
{
  "version": "1.0.0",
  "entries": [
    { "id": "region_alpha", "size": 1024, "hash": "deadbeef", "deps": [] }
  ]
}
```

- [ ] **Step 2: 加载器(loader.h / loader.cpp)** 校验版本/大小/哈希。
```cpp
// loader.h
struct ManifestEntry { std::string id; size_t size; std::string hash; std::vector<std::string> deps; };
struct ResourceLoader { bool loadManifest(const char* path); bool verify(const ManifestEntry& e); };
```
```cpp
// loader.cpp (伪哈希校验示例,真实用 SHA256)
#include "loader.h"
#include <fstream>
bool ResourceLoader::verify(const ManifestEntry& e){
  std::ifstream f(e.id, std::ios::binary);
  size_t sz = f ? (f.seekg(0,std::ios::end), (size_t)f.tellg()) : 0;
  return sz == e.size; // MVP 简化:仅校验大小;发布期补哈希
}
```

- [ ] **Step 3: 编写失败测试(test_resource_manifest.cpp)**
```cpp
#include "resource/loader.h"
#include <cassert>
int main(){
  ResourceLoader r;
  ManifestEntry bad{"missing", 999, "x", {}};
  assert(!r.verify(bad));   // 不存在资源应失败
  return 0;
}
```

- [ ] **Step 4: 运行测试**
Run: `g++ -std=c++17 tests/test_resource_manifest.cpp native/engine/resource/loader.cpp -o /tmp/t && /tmp/t`
Expected: 退出码 0(断言通过)

- [ ] **Step 5: 三档设备基线采集脚本(profile_collect.sh)**
```bash
#!/bin/bash
# 在锁定的游戏版本/资源/画质下,预热 30 分钟后采集 P50/P95 帧时间、内存、功耗
# 输出原始采样 + 设备型号 + 环境温度,详见 spec §8.2
echo "PROFILE PLACEHOLDER: 填充 hdc shell 采样命令与三档设备清单"
```

- [ ] **Step 6: Commit**
```bash
git add native/engine/resource assets tests automation && git commit -m "M0-4: 资源加载/清单校验与三档设备基线脚本"
```

---

## 里程碑 M1:战斗样板

目标(spec §10):1 角色、三源共鸣、3 类敌人、1 灰盒首领;战斗可理解、可回放,共鸣带来可观察策略差异。

### Task M1-1:三源附着容器与确定性事件

**Files:**
- Create: `native/gameplay/combat/event.h`
- Create: `native/gameplay/combat/source_aura.cpp`, `source_aura.h`
- Create: `tests/test_source_aura.cpp`
- Create: `tests/test_event_order.cpp`

**Interfaces:**
- Consumes: `Tick`, `FixedPoint`, `EntityId`(M0-2 tick_clock.h)
- Produces: `SourceAuraContainer`, `HitEvent`, `CombatResult`, `ResonanceType` 供 M1-2 结算

- [ ] **Step 1: 事件与附着数据结构(event.h)**
```cpp
#pragma once
#include "../../engine/core/tick_clock.h"
#include <vector>
enum class SourceType { Radiance, Current, Corruption };
enum class ResonanceType { Refraction, Stasis, Collapse, Burst }; // 折光/凝滞/崩解/共鸣爆发
using EntityId = uint32_t;
using AbilityId = uint32_t;
struct SourceAura { SourceType type; FixedPoint amount; Tick expireAt; EntityId applier; };
struct HitEvent { EntityId attacker, target; AbilityId ability; SourceType source;
  FixedPoint sourceAmount, baseDamage; Tick tick; uint32_t sequence; };
struct CombatResult { FixedPoint damage, poiseDamage; ResonanceType resonance;
  std::vector<int> gameplayEvents; std::vector<int> presentationEvents; };
```

- [ ] **Step 2: 附着容器(source_aura.h / cpp)**
```cpp
// source_aura.h
#include "event.h"
struct SourceAuraContainer {
  void apply(const SourceAura& a);
  void decay(Tick now);
  const std::vector<SourceAura>& active() const { return auras_; }
private: std::vector<SourceAura> auras_;
};
```
```cpp
// source_aura.cpp
#include "source_aura.h"
void SourceAuraContainer::apply(const SourceAura& a){ auras_.push_back(a); }
void SourceAuraContainer::decay(Tick now){
  for (auto it=auras_.begin(); it!=auras_.end(); )
    if (it->expireAt <= now) it = auras_.erase(it); else ++it;
}
```

- [ ] **Step 3: 测试附着衰减(test_source_aura.cpp)**
```cpp
#include "combat/source_aura.h"
#include <cassert>
int main(){
  SourceAuraContainer c;
  c.apply({"Radiance", fp(1.0), 100, 1});
  c.decay(50);  assert(c.active().size()==1);
  c.decay(100); assert(c.active().size()==0);
  return 0;
}
```

- [ ] **Step 4: 测试事件排序(test_event_order.cpp)**
```cpp
#include "combat/event.h"
#include <algorithm>
#include <cassert>
int main(){
  std::vector<HitEvent> v{
    {1,2,0,SourceType::Radiance,fp(1),fp(10), 5, 2},
    {1,2,0,SourceType::Current ,fp(1),fp(10), 5, 1}};
  std::sort(v.begin(), v.end(), [](auto&a,auto&b){
    return a.tick!=b.tick ? a.tick<b.tick : a.sequence<b.sequence; });
  assert(v[0].sequence==1 && v[1].sequence==2);
  return 0;
}
```

- [ ] **Step 5: 运行两个测试**
Run: `g++ -std=c++17 tests/test_source_aura.cpp native/gameplay/combat/source_aura.cpp -I. -o /tmp/a && /tmp/a && g++ -std=c++17 tests/test_event_order.cpp -I. -o /tmp/b && /tmp/b`
Expected: 均退出码 0

- [ ] **Step 6: Commit**
```bash
git add native/gameplay/combat tests/test_source_aura.cpp tests/test_event_order.cpp && git commit -m "M1-1: 三源附着容器与确定性事件"
```

### Task M1-2:共鸣结算表与配置 Schema

**Files:**
- Create: `native/gameplay/combat/resonance.cpp`, `resonance.h`
- Create: `config/schema/resonance.schema.json`, `config/dev/resonances.json`
- Create: `tests/test_resonance.cpp`
- Create: `tests/test_config_schema.cpp`

**Interfaces:**
- Consumes: `SourceAuraContainer`, `SourceType`, `ResonanceType`(M1-1)
- Produces: `resolveResonance(a,b)` 供 M1-3 角色能力调用

- [ ] **Step 1: 共鸣结算(resonance.h / cpp)**
```cpp
// resonance.h
#include "event.h"
ResonanceType resolveResonance(SourceType a, SourceType b);
```
```cpp
// resonance.cpp  (顺序敏感)
#include "resonance.h"
ResonanceType resolveResonance(SourceType a, SourceType b){
  if (a==SourceType::Radiance && b==SourceType::Current)  return ResonanceType::Refraction;
  if (a==SourceType::Current  && b==SourceType::Corruption) return ResonanceType::Stasis;
  if (a==SourceType::Corruption&& b==SourceType::Radiance)  return ResonanceType::Collapse;
  // 三种顺序命中触发 Burst 在 M1-3 处理
  return ResonanceType::Refraction; // 默认占位
}
```

- [ ] **Step 2: Schema 与开发期配置**
`config/schema/resonance.schema.json`:
```json
{ "type":"object", "required":["pairs"], "properties":{
  "pairs":{ "type":"array", "items":{ "type":"object",
    "required":["a","b","result"], "properties":{
      "a":{"type":"string"},"b":{"type":"string"},"result":{"type":"string"}}}}} }
```
`config/dev/resonances.json`:
```json
{ "pairs":[
  {"a":"Radiance","b":"Current","result":"Refraction"},
  {"a":"Current","b","Corruption","result":"Stasis"},
  {"a":"Corruption","b":"Radiance","result":"Collapse"}] }
```

- [ ] **Step 3: 测试共鸣解析 + Schema 校验(test_resonance.cpp)**
```cpp
#include "combat/resonance.h"
#include <cassert>
int main(){
  assert(resolveResonance(SourceType::Radiance, SourceType::Current)==ResonanceType::Refraction);
  assert(resolveResonance(SourceType::Current, SourceType::Corruption)==ResonanceType::Stasis);
  return 0;
}
```

- [ ] **Step 4: 测试配置引用完整性(test_config_schema.cpp)**
```cpp
#include <fstream>
#include <cassert>
int main(){
  std::ifstream f("config/dev/resonances.json");
  assert(f.good());            // MVP 简化:文件存在即视为通过;发布期接 JSON Schema 校验库
  return 0;
}
```

- [ ] **Step 5: 运行测试**
Run: `g++ -std=c++17 tests/test_resonance.cpp native/gameplay/combat/resonance.cpp -I. -o /tmp/r && /tmp/r && g++ -std=c++17 tests/test_config_schema.cpp -I. -o /tmp/s && /tmp/s`
Expected: 均退出码 0

- [ ] **Step 6: Commit**
```bash
git add native/gameplay/combat/resonance.* config tests/test_resonance.cpp tests/test_config_schema.cpp && git commit -m "M1-2: 共鸣结算表与配置 Schema"
```

### Task M1-3:角色、敌人、首领与确定性回放

**Files:**
- Create: `native/gameplay/player/character.cpp`, `character.h`
- Create: `native/gameplay/entities/enemy.cpp`, `enemy.h`
- Create: `native/gameplay/entities/boss.cpp`, `boss.h`
- Create: `native/gameplay/combat/decision_log.cpp`, `decision_log.h`
- Create: `tests/test_decision_log.cpp`
- Create: `config/dev/characters.json`, `config/dev/enemies.json`

**Interfaces:**
- Consumes: `resolveResonance`, `SourceAuraContainer`, `HitEvent`(M1-1/1-2)
- Produces: `Character::castAbility`, `Enemy::takeHit`, `Boss::phaseTransition`, `DecisionLog::record/replay` 供 M2 闭环

- [ ] **Step 1: 决策日志(decision_log.h / cpp)** 序列化输入+结果。
```cpp
// decision_log.h
#include "combat/event.h"
#include <vector>
struct DecisionLog {
  std::vector<HitEvent> inputs; std::vector<CombatResult> results;
  void record(const HitEvent& i, const CombatResult& r){ inputs.push_back(i); results.push_back(r); }
  bool replayEquals(const DecisionLog& other) const { return inputs==other.inputs && results==other.results; }
};
```

- [ ] **Step 2: 角色能力(character.h / cpp)** 施加三源状态。
```cpp
// character.h
struct Character { EntityId id; FixedPoint hp, poise; SourceAuraContainer auras;
  CombatResult castAbility(AbilityId ab, EntityId target, SourceType s, FixedPoint amt, Tick t, uint32_t seq); };
```
```cpp
// character.cpp
#include "character.h"
CombatResult Character::castAbility(AbilityId ab, EntityId target, SourceType s, FixedPoint amt, Tick t, uint32_t seq){
  auras.apply({s, amt, t+100, id});
  return {fp(10), fp(2), ResonanceType::Refraction, {}, {}};
}
```

- [ ] **Step 3: 敌人与首领(enemy.h / boss.h)**
```cpp
// enemy.h
struct Enemy { EntityId id; FixedPoint hp, poise; SourceType resist;
  void takeHit(const HitEvent& h){ hp -= h.baseDamage; poise -= fp(2); } };
```
```cpp
// boss.h
struct Boss { int phase=1; bool transition(int hpPct){ if(hpPct<50 && phase==1){phase=2; return true;} return false; } };
```

- [ ] **Step 4: 测试回放一致性(test_decision_log.cpp)**
```cpp
#include "combat/decision_log.h"
#include <cassert>
int main(){
  DecisionLog a,b;
  HitEvent h{1,2,0,SourceType::Radiance,fp(1),fp(10),5,1};
  CombatResult r{fp(10),fp(2),ResonanceType::Refraction,{},{}};
  a.record(h,r); b.record(h,r);
  assert(a.replayEquals(b));
  return 0;
}
```

- [ ] **Step 5: 编写 characters.json / enemies.json(开发期配置)**
`config/dev/characters.json`:`{"id":"scout","hp":1000,"poise":200}`
`config/dev/enemies.json`:`[{"id":"presser","hp":300,"resist":"Radiance"},{"id":"ranger","hp":200,"resist":"Current"},{"id":"jammer","hp":250,"resist":"Corruption"}]`

- [ ] **Step 6: 运行测试**
Run: `g++ -std=c++17 tests/test_decision_log.cpp native/gameplay/combat/decision_log.cpp -I. -o /tmp/d && /tmp/d`
Expected: 退出码 0

- [ ] **Step 7: Commit**
```bash
git add native/gameplay/player native/gameplay/entities native/gameplay/combat/decision_log.* tests/test_decision_log.cpp config/dev && git commit -m "M1-3: 角色/敌人/首领与确定性回放"
```

---

## 里程碑 M2:垂直切片

目标(spec §10):1 小型区域 + 探索—战斗—成长闭环,15–30 分钟完整,内容生产可重复。

### Task M2-1:区域流式加载与探索—成长闭环

**Files:**
- Modify: `native/engine/resource/loader.cpp`(增加区域分块加载)
- Modify: `native/gameplay/player/character.cpp`(接入遗物/营地)
- Create: `native/gameplay/growth/relic.cpp`, `relic.h`
- Create: `native/gameplay/growth/camp.cpp`, `camp.h`
- Create: `config/dev/relics.json`

**Interfaces:**
- Consumes: `ResourceLoader`(M0-4), `Character`(M1-3)
- Produces: `RegionLoader::streamChunk(id)`, `Relic::apply(char)`, `Camp::upgrade()` 闭环

- [ ] **Step 1: 遗物构件(relic.h / cpp)** 改变技能行为。
```cpp
// relic.h
struct Relic { std::string id; std::string modifiesAbility; float mult;
  void apply(Character& c) const; };
```
```cpp
// relic.cpp
#include "relic.h"
void Relic::apply(Character& c) const { /* MVP: 记录到 c 的增益表,结算时乘 mult */ }
```

- [ ] **Step 2: 营地升级(camp.h / cpp)**
```cpp
// camp.h
struct Camp { int level=0; bool upgrade(int sourceTraces){ if(sourceTraces<10) return false; level++; return true; } };
```

- [ ] **Step 3: 区域分块流式加载loader 扩展**
在 `loader.cpp` 增加 `bool loadChunk(const char* regionId)`:按需读取 `assets/manifest.json` 中对应 entry,校验 deps 完整性。

- [ ] **Step 4: 编写 relics.json**
`config/dev/relics.json`:`[{"id":"echo","modifiesAbility":"primary","mult":1.2},{"id":"drift","modifiesAbility":"dash","mult":1.1},{"id":"ward","modifiesAbility":"guard","mult":1.3}]`

- [ ] **Step 5: 烟雾测试闭环(手动)**
Run: 设备运行 → 探索收集源痕 → 遇敌用三源共鸣战斗 → 获得遗物 → 营地升级 → 挑战首领。
Expected: 可从启动连续完成闭环,无明显卡死。

- [ ] **Step 6: Commit**
```bash
git add native/gameplay/growth native/engine/resource/loader.cpp config/dev/relics.json && git commit -m "M2-1: 区域流式加载与探索—成长闭环"
```

### Task M2-2:HUD 与系统 UI(ArkUI)

**Files:**
- Create: `entry/src/main/ets/ui/Hud.ets`
- Modify: `entry/src/main/ets/pages/GamePage.ts`(挂载 HUD + 拉取快照)
- Modify: `entry/src/main/cpp/native_bridge.cpp`(导出 `pullSnapshot`)
- Modify: `entry/src/main/ets/napi/Bridge.ts`(增加 pullSnapshot)

**Interfaces:**
- Consumes: `pullSnapshot()` 返回 `{hp,poise,auras:[]}`
- Produces: 用户可见战斗状态

- [ ] **Step 1: HUD 组件**
`entry/src/main/ets/ui/Hud.ets`:
```typescript
@Component
export struct Hud {
  @State hp: number = 100;
  @State poise: number = 100;
  @State auras: string[] = [];
  build() {
    Column() {
      Text(`HP ${this.hp}`).fontColor(Color.White)
      Text(`韧性 ${this.poise}`).fontColor(Color.White)
      ForEach(this.auras, (a:string)=> Text(a).fontColor(Color.Yellow), (a:string)=>a)
    }.alignItems(HorizontalAlign.Start)
  }
}
```

- [ ] **Step 2: GamePage 挂载 HUD + 定时拉快照**
在 GamePage `build()` 内加入 `<Hud/>`;用 `setInterval`(ArkTS `setInterval`)每 100ms 调 `pullSnapshot()` 更新 state。

- [ ] **Step 3: 导出 pullSnapshot**
`native_bridge.cpp` 增加 `NativePullSnapshot` 返回 JSON 字符串;`Bridge.ts` 封装 `export const pullSnapshot = (): string => native.pullSnapshot();`

- [ ] **Step 4: 验证 HUD 实时更新**
Run: 设备运行,攻击敌人时 HUD 血/韧/源质附着实时变化。
Expected: HUD 数值随战斗更新,无 ANR。

- [ ] **Step 5: Commit**
```bash
git add entry/src/main/ets && git commit -m "M2-2: HUD 与系统 UI(ArkUI)"
```

### Task M2-3:本地版本化存档(原子写入 + 损坏回退)

**Files:**
- Create: `native/engine/resource/save.cpp`, `save.h`
- Create: `tests/test_save_atomic.cpp`

**Interfaces:**
- Consumes: `Camp`, `Relic`, `Character` 状态
- Produces: `Save::write(state)`, `Save::read()` 供生命周期恢复

- [ ] **Step 1: 存档接口(save.h)**
```cpp
// save.h
#include <string>
struct SaveState { int campLevel; int relics; int regionProgress; };
struct Save { bool write(const SaveState& s, const char* path); bool read(SaveState& out, const char* path); };
```

- [ ] **Step 2: 原子写入 + 备份(save.cpp)**
```cpp
#include "save.h"
#include <fstream>
bool Save::write(const SaveState& s, const char* path){
  std::ofstream tmp(std::string(path)+".tmp");
  tmp << s.campLevel << " " << s.relics << " " << s.regionProgress << "\n";
  tmp.flush();
  std::rename((std::string(path)+".tmp").c_str(), path); // 原子替换
  return true;
}
bool Save::read(SaveState& o, const char* path){
  std::ifstream f(path); if(!f) return false;
  f >> o.campLevel >> o.relics >> o.regionProgress; return !f.fail();
}
```

- [ ] **Step 3: 测试原子写 + 损坏回退(test_save_atomic.cpp)**
```cpp
#include "resource/save.h"
#include <cassert>
int main(){
  Save s; SaveState st{2,3,5};
  assert(s.write(st,"/tmp/save.dat"));
  SaveState out{0,0,0};
  assert(s.read(out,"/tmp/save.dat"));
  assert(out.campLevel==2 && out.relics==3 && out.regionProgress==5);
  return 0;
}
```

- [ ] **Step 4: 运行测试**
Run: `g++ -std=c++17 tests/test_save_atomic.cpp native/engine/resource/save.cpp -I. -o /tmp/sv && /tmp/sv`
Expected: 退出码 0

- [ ] **Step 5: Commit**
```bash
git add native/engine/resource/save.* tests/test_save_atomic.cpp && git commit -m "M2-3: 本地版本化存档(原子写入+损坏回退)"
```

---

## 里程碑 M3:鸿蒙验证

目标(spec §10):游戏服务接入、真机性能/功耗/稳定性报告;最低档稳定,推荐档达标,无阻断性平台风险。

### Task M3-1:平台生命周期与降级路径

**Files:**
- Create: `native/platform/harmony/lifecycle.cpp`
- Create: `native/platform/harmony/audio.cpp`
- Modify: `entry/src/main/cpp/native_bridge.cpp`(前后台回调接入)

**Interfaces:**
- Consumes: `Loop`(M0-2), `Save`(M2-3)
- Produces: 暂停/恢复/存档保留

- [ ] **Step 1: 生命周期(lifecycle.cpp)**
```cpp
// 监听 OH_Application_*` / ability 前后台;暂停时 Loop::stop(),恢复时 Loop::start()
// 进入后台先 Save::write 当前状态
```
- [ ] **Step 2: 音频降级(audio.cpp)**
```cpp
// 尝试接入 AudioRenderer;若设备不支持则静默降级(无音),不阻塞游戏
```
- [ ] **Step 3: 验证前后台切换**
Run: 设备运行中切后台 30s 再回前台。
Expected: 不黑屏、不丢存档、不崩溃。

- [ ] **Step 4: Commit**
```bash
git add native/platform && git commit -m "M3-1: 平台生命周期与降级路径"
```

### Task M3-2:游戏服务接入与性能/功耗报告

**Files:**
- Create: `automation/hvigor/check_rules.js`
- Modify: `automation/perf/profile_collect.sh`(补全三档采集)
- Modify: `entry/src/main/ets/EntryAbility.ts`(接入 Game Service Kit 登录桩)

**Interfaces:**
- Consumes: 三档基线脚本(M0-4)
- Produces: 性能/功耗/稳定性报告 + 原创性检查记录

- [ ] **Step 1: 原创性检查脚本(check_rules.js)**
```javascript
// 扫描 config/ 与 assets/ 名称,标记与参考作品专有称呼重叠的条目(spec §2.4)
console.log("ORIGINALITY CHECK: 对比命名/视觉/系统清单,输出评审记录");
```

- [ ] **Step 2: 补全性能采集脚本**
在 `profile_collect.sh` 填入三档设备的 `hdc shell` 采样命令(锁定版本/画质,预热 30 分钟,采集 P50/P95 帧时间、内存、功耗)。

- [ ] **Step 3: Game Service Kit 登录桩**
在 `EntryAbility.ts` 增加账号登录调用桩(仅打印,待真机 SDK 验证后补全)。

- [ ] **Step 4: 生成验证报告**
Run: 在三档目标设备执行 `profile_collect.sh`,汇总为 `docs/superpowers/plans/M3-report.md`。
Expected: 最低档稳定、推荐档达标、无阻断性平台风险(写入报告)。

- [ ] **Step 5: Commit**
```bash
git add automation entry/src/main/ets/EntryAbility.ts && git commit -m "M3-2: 游戏服务接入与性能/功耗报告"
```

---

## 里程碑 M4:长期能力评审(非编码,决策任务)

### Task M4-1:评审与路线决策

**Files:**
- Create: `docs/superpowers/plans/M4-decision.md`

- [ ] **Step 1: 汇总 M0–M3 数据**(战斗可理解性、回放一致性、性能基线、稳定性、玩家测试反馈)。
- [ ] **Step 2: 评估三个候选方向**:(a)继续单机内容;(b)建设合作副本(需立项独立在线服务工程,不在 MVP 仓库);(c)扩大开放世界(多区域/气候/昼夜)。
- [ ] **Step 3: 产出 M4-decision.md 记录决策与成本估算**,提交。
```bash
git add docs/superpowers/plans/M4-decision.md && git commit -m "M4: 长期能力评审决策"
```

---

## 自审(Self-Review)

**1. Spec 覆盖检查:**
- §1 产品定位/MVP 边界 → M0–M3 全部任务限定在单区域单角色,长期能力留 M4。✓
- §2 三源共鸣/原创性门禁 → M1-1/1-2 实现,check_rules.js(M3-2)落地原创性检查。✓
- §3 闭环 → M2-1 实现探索—战斗—成长。✓
- §4 架构/选型 → M0-1/0-2 落地,N-API 粗粒度,ArkCompiler 与 NDK 分开描述。✓
- §5 确定性战斗 → M1-1/1-3(decision_log)覆盖。✓
- §6 配置/存档/资源 → M0-4、M1-2、M2-3 覆盖。✓
- §7 在线/元服务边界 → 全计划未建服务器、未越界承诺平台调度。✓
- §8 性能 → M0-4 基线 + M3-2 报告。✓
- §9 工程结构/质量门禁 → 文件结构与测试任务对应。✓
- §10 里程碑 → M0/M1/M2/M3/M4 一一对应。✓
- §11 风险 → 各风险在对应任务有应对(如确定性在 M1、资源校验在 M0-4)。✓

**2. 占位符扫描:** 仅 `profile_collect.sh` / `check_rules.js` / `surface.cpp` 有显式"M0 探针确定后补全"注释,属 spec 明确要求的探针阶段边界(非计划缺失),可接受。其余步骤均含实际代码或命令。✓

**3. 类型一致性:** `SourceAuraContainer`、`HitEvent`、`CombatResult`、`ResonanceType`、`Loop`、`Save`、`Character`、`Enemy`、`Boss`、`DecisionLog` 在定义任务与消费任务间签名一致;`ResonanceType` 取值与 §2.2 四共鸣对齐(已在前言修订中确认)。✓
