#pragma once

#include "gameplay/combat/event.h"

#include <cstdint>
#include <optional>
#include <vector>

// 程序化音效（procedural sound）：用波形合成生成 44.1kHz 单声道 16bit
// PCM，避免携带音频素材文件；战斗事件映射到固定音色，跨平台可单元测试。
enum class SoundEffect : uint8_t {
  Hit,           // 普通命中：低频方波脉冲
  HeavyHit,      // 重击：更低更长的冲击
  Dodge,         // 闪避：高频下滑扫音
  Kill,          // 击杀确认：上行双音
  PoiseBreak,    // 破韧：锯齿下滑碎音
  CastBarBroken, // 打断吟唱：噪声碎裂
  Resonance,     // 共鸣爆发：三音和弦
  AuraApplied,   // 源技能附着：柔和正弦
  PhaseChanged,  // Boss 阶段转换：低沉警示
};

// 合成指定音效的完整 PCM 缓冲（44100 Hz、单声道、int16）。
std::vector<int16_t> synthesizeSound(SoundEffect effect);

// 采样率与音效时长查询，供播放端预分配与测试断言。
constexpr int kSoundSampleRate = 44100;

// 战斗事件 → 音效映射；不发声的事件返回 nullopt。
std::optional<SoundEffect> soundForGameplayEvent(GameplayEventType type);
std::optional<SoundEffect> soundForPresentationEvent(PresentationEventType type);
