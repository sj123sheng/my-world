#include "audio_bridge.h"

#include <algorithm>

#ifdef OHOS_PLATFORM
#include <ohaudio/native_audiostreambuilder.h>
#endif

namespace {

// 大额伤害阈值：与伤害飘字 Heavy 判定一致，升级为重击音色。
constexpr float kHeavyDamageThreshold = 15.0f;

}  // namespace

#ifdef OHOS_PLATFORM
namespace {

// 音频回调：把活跃声部混音写入输出缓冲（16bit 单声道）。
int32_t rendererOnWrite(OH_AudioRenderer* /*renderer*/, void* userData,
                        void* buffer, int32_t length) {
  AudioBridge* bridge = static_cast<AudioBridge*>(userData);
  const int32_t frameCount = length / static_cast<int32_t>(sizeof(int16_t));
  return bridge->fillBuffer(static_cast<int16_t*>(buffer), frameCount);
}

}  // namespace
#endif

AudioBridge::~AudioBridge() {
  stop();
}

void AudioBridge::start() {
#ifdef OHOS_PLATFORM
  OH_AudioStreamBuilder* builder = nullptr;
  if (OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER) !=
          AUDIOSTREAM_SUCCESS ||
      builder == nullptr) {
    return;  // 静默降级：无音。
  }
  OH_AudioStreamBuilder_SetSamplingRate(builder, kSoundSampleRate);
  OH_AudioStreamBuilder_SetChannelCount(builder, 1);
  OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE);
  OH_AudioStreamBuilder_SetEncodingType(builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
  OH_AudioStreamBuilder_SetLatencyMode(builder,
                                       AUDIOSTREAM_LATENCY_MODE_FAST);
  OH_AudioRenderer_Callbacks callbacks{};
  callbacks.OH_AudioRenderer_OnWriteData = rendererOnWrite;
  OH_AudioStreamBuilder_SetRendererCallback(builder, callbacks, this);
  const OH_AudioStream_Result generated =
      OH_AudioStreamBuilder_GenerateRenderer(builder, &renderer_);
  OH_AudioStreamBuilder_Destroy(builder);
  if (generated != AUDIOSTREAM_SUCCESS || renderer_ == nullptr) {
    renderer_ = nullptr;
    return;  // 静默降级：无音。
  }
  if (OH_AudioRenderer_Start(renderer_) != AUDIOSTREAM_SUCCESS) {
    OH_AudioRenderer_Release(renderer_);
    renderer_ = nullptr;
    return;
  }
  initialized_ = true;
  startAmbient();  // 初始化成功即开始环境垫底音乐。
#else
  // 非平台侧不发声，但保持映射/声部状态可测。
  initialized_ = false;
#endif
}

void AudioBridge::stop() {
  stopAmbient();
#ifdef OHOS_PLATFORM
  if (renderer_ != nullptr) {
    OH_AudioRenderer_Stop(renderer_);
    OH_AudioRenderer_Release(renderer_);
    renderer_ = nullptr;
  }
#endif
  initialized_ = false;
}

void AudioBridge::dispatch(const CombatEventBatch& batch) {
  if (!enabled_) return;
  lastDispatched_.clear();
  for (const GameplayEvent& event : batch.gameplay) {
    std::optional<SoundEffect> effect = soundForGameplayEvent(event.type);
    if (event.type == GameplayEventType::Damage) {
      const float amount =
          static_cast<float>(event.value) / static_cast<float>(FP_ONE);
      if (amount >= kHeavyDamageThreshold) effect = SoundEffect::HeavyHit;
    }
    if (effect.has_value()) {
      lastDispatched_.push_back(*effect);
      play(*effect);
    }
  }
  for (const PresentationEvent& event : batch.presentation) {
    const std::optional<SoundEffect> effect =
        soundForPresentationEvent(event.type);
    if (effect.has_value()) {
      lastDispatched_.push_back(*effect);
      play(*effect);
    }
  }
}

void AudioBridge::setEnabled(bool enabled) {
  if (enabled_ == enabled) return;
  enabled_ = enabled;
  if (enabled) {
    if (initialized_) startAmbient();
  } else {
    stopAmbient();
    std::lock_guard<std::mutex> lock(mutex_);
    for (Voice& voice : voices_) voice.active = false;
    ambientVoice_ = -1;
  }
}

void AudioBridge::startAmbient() {
  play(SoundEffect::Ambient, true);
}

void AudioBridge::stopAmbient() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (ambientVoice_ >= 0 && ambientVoice_ < kVoiceCount) {
    voices_[static_cast<size_t>(ambientVoice_)].active = false;
    voices_[static_cast<size_t>(ambientVoice_)].looping = false;
  }
  ambientVoice_ = -1;
}

void AudioBridge::play(SoundEffect effect, bool looping) {
  const int index = static_cast<int>(effect);
  if (index < 0 || index >= kSoundEffectCount) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!synthesized_[static_cast<size_t>(index)]) {
    samples_[static_cast<size_t>(index)] = synthesizeSound(effect);
    synthesized_[static_cast<size_t>(index)] = true;
  }
  if (looping && ambientVoice_ >= 0 && ambientVoice_ < kVoiceCount) {
    return;  // 幂等：环境音垫已在播放。
  }
  int slot = -1;
  for (int i = 0; i < kVoiceCount; ++i) {
    if (!voices_[static_cast<size_t>(i)].active) {
      slot = i;
      break;
    }
  }
  if (slot < 0) {
    if (looping) return;  // 无空闲声部时不抢占一次性音效来放垫底。
    // 声部占满：轮转抢占最旧的非循环声部，保证新反馈永远可闻。
    for (int probe = 0; probe < kVoiceCount; ++probe) {
      const int candidate = (nextStealVoice_ + probe) % kVoiceCount;
      if (!voices_[static_cast<size_t>(candidate)].looping) {
        slot = candidate;
        break;
      }
    }
    if (slot < 0) return;  // 仅剩循环声部，不抢占。
    nextStealVoice_ = (slot + 1) % kVoiceCount;
  }
  voices_[static_cast<size_t>(slot)] = {index, 0, true, looping};
  if (looping) ambientVoice_ = slot;
}

int32_t AudioBridge::fillBuffer(int16_t* output, int32_t frameCount) {
  if (output == nullptr || frameCount <= 0) return 0;
  if (!enabled_) {
    std::fill(output, output + frameCount, static_cast<int16_t>(0));
    return frameCount * static_cast<int32_t>(sizeof(int16_t));
  }
  std::lock_guard<std::mutex> lock(mutex_);
  for (int32_t frame = 0; frame < frameCount; ++frame) {
    int32_t mix = 0;
    bool anyActive = false;
    for (Voice& voice : voices_) {
      if (!voice.active) continue;
      const std::vector<int16_t>& pcm =
          samples_[static_cast<size_t>(voice.effect)];
      if (voice.offset >= pcm.size()) {
        if (voice.looping && !pcm.empty()) {
          voice.offset = 0;  // 循环声部回绕重播。
        } else {
          voice.active = false;
          continue;
        }
      }
      mix += pcm[voice.offset];
      ++voice.offset;
      anyActive = true;
    }
    output[frame] = anyActive || mix != 0
                        ? static_cast<int16_t>(std::clamp(mix, -32768, 32767))
                        : 0;
  }
  return frameCount * static_cast<int32_t>(sizeof(int16_t));
}
