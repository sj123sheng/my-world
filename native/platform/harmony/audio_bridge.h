#pragma once

#include "engine/presentation/procedural_sound.h"
#include "gameplay/combat/combat_controller.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

#ifdef OHOS_PLATFORM
#include <ohaudio/native_audiorenderer.h>
#endif

// AudioBridge 把战斗事件映射为程序化合成音效并实时混音播放。
// 非平台侧维护同样的映射/声部状态供宿主机测试，但不发声；
// OHOS 侧通过 OHAudio 低延迟渲染流输出，创建失败时静默降级（无音）。
class AudioBridge {
 public:
  static constexpr int kSoundEffectCount = 9;
  static constexpr int kVoiceCount = 8;

  AudioBridge() = default;
  ~AudioBridge();

  AudioBridge(const AudioBridge&) = delete;
  AudioBridge& operator=(const AudioBridge&) = delete;

  // 初始化音频渲染流。不支持/失败时静默降级为无音。
  void start();

  // 停止并释放音频渲染流。
  void stop();

  // 按批内事件映射并触发音效；大额伤害升级为重击音色。
  void dispatch(const CombatEventBatch& batch);

  // 最近一次 dispatch 触发的音效序列（按事件顺序），供宿主机状态测试。
  const std::vector<SoundEffect>& lastDispatched() const {
    return lastDispatched_;
  }

  bool initialized() const { return initialized_; }

  // 混音填充：把活跃声部叠加进输出缓冲（frameCount 为采样帧数）。
  // 由音频回调线程调用（OHOS 侧），内部持锁保护声部状态。
  int32_t fillBuffer(int16_t* output, int32_t frameCount);

 private:
  struct Voice {
    int effect = -1;
    size_t offset = 0;
    bool active = false;
  };

  // 触发一个音效：懒合成 PCM 缓存，占用空闲声部，无空闲时轮转抢占。
  void play(SoundEffect effect);

  bool initialized_ = false;
  std::mutex mutex_;
  std::vector<SoundEffect> lastDispatched_;
  std::array<std::vector<int16_t>, kSoundEffectCount> samples_;
  std::array<bool, kSoundEffectCount> synthesized_{};
  std::array<Voice, kVoiceCount> voices_{};
  int nextStealVoice_ = 0;

#ifdef OHOS_PLATFORM
  OH_AudioRenderer* renderer_ = nullptr;
#endif
};
