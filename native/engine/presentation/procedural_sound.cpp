#include "native/engine/presentation/procedural_sound.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace {

constexpr float kTwoPi = 6.2831853071795864769f;

// 确定性 LCG 噪声源：同音效每次合成结果一致，便于测试断言。
struct NoiseGen {
  uint32_t seed = 0x2545F491u;
  float next() {
    seed = seed * 1664525u + 1013904223u;
    return static_cast<float>((seed >> 16) & 0x7FFFu) / 32767.5f - 0.5f;
  }
};

float squareWave(float phase01) { return phase01 < 0.5f ? 1.0f : -1.0f; }

float sawWave(float phase01) { return 2.0f * phase01 - 1.0f; }

// 通用渲染：按采样函数生成 PCM；采样函数接收秒数 t 与归一化进度 t01。
std::vector<int16_t> render(
    float durationSeconds,
    const std::function<float(float t, float t01)>& sample) {
  const int count =
      static_cast<int>(durationSeconds * static_cast<float>(kSoundSampleRate));
  std::vector<int16_t> pcm(static_cast<size_t>(count));
  for (int i = 0; i < count; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(kSoundSampleRate);
    const float t01 = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1)
                                : 1.0f;
    const float value = std::clamp(sample(t, t01), -1.0f, 1.0f);
    pcm[static_cast<size_t>(i)] = static_cast<int16_t>(value * 32000.0f);
  }
  return pcm;
}

// 普通命中：90Hz 方波脉冲 + 起音噪声，快速指数衰减。
std::vector<int16_t> synthesizeHit() {
  NoiseGen noise;
  return render(0.07f, [&](float t, float t01) {
    const float env = std::exp(-6.0f * t01) * 0.8f;
    const float phase = std::fmod(90.0f * t, 1.0f);
    const float click = t01 < 0.08f ? noise.next() * 1.6f : 0.0f;
    return squareWave(phase) * env + click * (1.0f - t01);
  });
}

// 重击：55Hz 更长更沉的冲击。
std::vector<int16_t> synthesizeHeavyHit() {
  NoiseGen noise;
  return render(0.13f, [&](float t, float t01) {
    const float env = std::exp(-4.5f * t01) * 0.9f;
    const float phase = std::fmod(55.0f * t, 1.0f);
    const float click = t01 < 0.06f ? noise.next() * 2.0f : 0.0f;
    return squareWave(phase) * env + click * (1.0f - t01);
  });
}

// 闪避：700→180Hz 正弦下滑扫音，轻盈短促。
std::vector<int16_t> synthesizeDodge() {
  return render(0.09f, [&](float t, float t01) {
    const float freq = 700.0f - 520.0f * t01;
    const float env = std::sin(3.14159265f * t01) * 0.5f;
    return std::sin(kTwoPi * freq * t) * env;
  });
}

// 击杀确认：523→784Hz 上行双音（纯五度），清晰的正反馈。
std::vector<int16_t> synthesizeKill() {
  return render(0.22f, [&](float t, float t01) {
    const bool second = t01 >= 0.5f;
    const float freq = second ? 784.0f : 523.25f;
    const float segmentT01 = second ? (t01 - 0.5f) * 2.0f : t01 * 2.0f;
    const float env = std::exp(-3.0f * segmentT01) * 0.55f;
    return std::sin(kTwoPi * freq * t) * env;
  });
}

// 破韧：280→70Hz 锯齿下滑，碎裂感。
std::vector<int16_t> synthesizePoiseBreak() {
  NoiseGen noise;
  return render(0.2f, [&](float t, float t01) {
    const float freq = 280.0f - 210.0f * t01;
    const float phase = std::fmod(freq * t, 1.0f);
    const float env = std::exp(-3.5f * t01) * 0.6f;
    return sawWave(phase) * env + noise.next() * 0.15f * (1.0f - t01);
  });
}

// 打断吟唱：噪声碎裂 + 150Hz 下滑正弦。
std::vector<int16_t> synthesizeCastBarBroken() {
  NoiseGen noise;
  return render(0.18f, [&](float t, float t01) {
    const float env = std::exp(-4.0f * t01);
    const float freq = 150.0f - 70.0f * t01;
    return (noise.next() * 0.9f + std::sin(kTwoPi * freq * t) * 0.5f) * env;
  });
}

// 共鸣爆发：C5/E5/G5 三音和弦，缓慢衰减。
std::vector<int16_t> synthesizeResonance() {
  return render(0.32f, [&](float, float t01) {
    const float env = std::exp(-2.5f * t01) * 0.35f;
    const float t = t01 * 0.32f;
    return (std::sin(kTwoPi * 523.25f * t) + std::sin(kTwoPi * 659.25f * t) +
            std::sin(kTwoPi * 783.99f * t)) *
           env;
  });
}

// 源技能附着：330Hz 柔和正弦短音。
std::vector<int16_t> synthesizeAuraApplied() {
  return render(0.12f, [&](float t, float t01) {
    const float env = std::sin(3.14159265f * t01) * 0.45f;
    return std::sin(kTwoPi * 330.0f * t) * env;
  });
}

// Boss 阶段转换：110Hz 方波 + 4Hz 颤音，低沉警示。
std::vector<int16_t> synthesizePhaseChanged() {
  return render(0.3f, [&](float t, float t01) {
    const float phase = std::fmod(110.0f * t, 1.0f);
    const float tremolo = 0.7f + 0.3f * std::sin(kTwoPi * 4.0f * t);
    const float env = std::exp(-2.0f * t01) * 0.7f;
    return squareWave(phase) * env * tremolo;
  });
}

}  // namespace

std::vector<int16_t> synthesizeSound(SoundEffect effect) {
  switch (effect) {
    case SoundEffect::Hit: return synthesizeHit();
    case SoundEffect::HeavyHit: return synthesizeHeavyHit();
    case SoundEffect::Dodge: return synthesizeDodge();
    case SoundEffect::Kill: return synthesizeKill();
    case SoundEffect::PoiseBreak: return synthesizePoiseBreak();
    case SoundEffect::CastBarBroken: return synthesizeCastBarBroken();
    case SoundEffect::Resonance: return synthesizeResonance();
    case SoundEffect::AuraApplied: return synthesizeAuraApplied();
    case SoundEffect::PhaseChanged: return synthesizePhaseChanged();
  }
  return {};
}

std::optional<SoundEffect> soundForGameplayEvent(GameplayEventType type) {
  switch (type) {
    case GameplayEventType::Damage: return SoundEffect::Hit;
    case GameplayEventType::Death: return SoundEffect::Kill;
    case GameplayEventType::Dodge: return SoundEffect::Dodge;
    case GameplayEventType::PoiseBreak: return SoundEffect::PoiseBreak;
    case GameplayEventType::Resonance: return SoundEffect::Resonance;
    case GameplayEventType::AuraApplied: return SoundEffect::AuraApplied;
    case GameplayEventType::PhaseChanged: return SoundEffect::PhaseChanged;
    case GameplayEventType::Hit:
    case GameplayEventType::Interrupt:
    case GameplayEventType::EncounterReset:
      return std::nullopt;
  }
  return std::nullopt;
}

std::optional<SoundEffect> soundForPresentationEvent(
    PresentationEventType type) {
  switch (type) {
    case PresentationEventType::CastBarBroken:
      return SoundEffect::CastBarBroken;
    case PresentationEventType::HitFlash:
    case PresentationEventType::CameraShake:
    case PresentationEventType::DodgeFlash:
    case PresentationEventType::PoiseBreakBurst:
    case PresentationEventType::ResonanceBurst:
    case PresentationEventType::PhaseTransition:
      return std::nullopt;
  }
  return std::nullopt;
}
