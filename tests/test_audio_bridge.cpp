#include "native/platform/harmony/audio_bridge.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

void testDispatchEmptyBatchDoesNotCrash() {
  AudioBridge audio;
  CombatEventBatch batch{};
  audio.dispatch(batch);
  assert(true);
}

void testDispatchWithEventsDoesNotCrash() {
  AudioBridge audio;
  CombatEventBatch batch{};
  GameplayEvent ge{};
  ge.type = GameplayEventType::Hit;
  batch.gameplay.push_back(ge);
  PresentationEvent pe{};
  pe.type = PresentationEventType::HitFlash;
  batch.presentation.push_back(pe);
  audio.dispatch(batch);
  assert(true);
}

void testStartStopDoesNotCrash() {
  AudioBridge audio;
  audio.start();
  audio.stop();
  assert(true);
}

void testMultipleDispatchesDoesNotCrash() {
  AudioBridge audio;
  audio.start();
  CombatEventBatch batch{};
  GameplayEvent ge{};
  ge.type = GameplayEventType::Dodge;
  batch.gameplay.push_back(ge);
  for (int i = 0; i < 100; i++) audio.dispatch(batch);
  audio.stop();
  assert(true);
}

// 事件→音效映射：普通伤害、击杀、闪避、吟唱打断。
void testEventMapping() {
  AudioBridge audio;
  CombatEventBatch batch{};
  GameplayEvent damage{};
  damage.type = GameplayEventType::Damage;
  damage.value = fp(8);
  batch.gameplay.push_back(damage);
  GameplayEvent death{};
  death.type = GameplayEventType::Death;
  batch.gameplay.push_back(death);
  GameplayEvent dodge{};
  dodge.type = GameplayEventType::Dodge;
  batch.gameplay.push_back(dodge);
  GameplayEvent silent{};
  silent.type = GameplayEventType::EncounterReset;
  batch.gameplay.push_back(silent);
  PresentationEvent castBroken{};
  castBroken.type = PresentationEventType::CastBarBroken;
  batch.presentation.push_back(castBroken);
  audio.dispatch(batch);
  const std::vector<SoundEffect>& played = audio.lastDispatched();
  assert(played.size() == 4);
  assert(played[0] == SoundEffect::Hit);
  assert(played[1] == SoundEffect::Kill);
  assert(played[2] == SoundEffect::Dodge);
  assert(played[3] == SoundEffect::CastBarBroken);
}

// 大额伤害升级为重击音色。
void testHeavyDamageUpgrade() {
  AudioBridge audio;
  CombatEventBatch batch{};
  GameplayEvent heavy{};
  heavy.type = GameplayEventType::Damage;
  heavy.value = fp(18);
  batch.gameplay.push_back(heavy);
  audio.dispatch(batch);
  assert(audio.lastDispatched().size() == 1);
  assert(audio.lastDispatched()[0] == SoundEffect::HeavyHit);
}

// 混音：触发音效后输出非零采样，播完后恢复静音。
void testMixingProducesThenSilence() {
  AudioBridge audio;
  CombatEventBatch batch{};
  GameplayEvent damage{};
  damage.type = GameplayEventType::Damage;
  damage.value = fp(8);
  batch.gameplay.push_back(damage);
  audio.dispatch(batch);
  std::vector<int16_t> buffer(512, 0);
  bool anyNonZero = false;
  // Hit 音效约 0.07s ≈ 3087 帧；填充足够多帧覆盖整个音效。
  for (int chunk = 0; chunk < 8; ++chunk) {
    audio.fillBuffer(buffer.data(), static_cast<int32_t>(buffer.size()));
    for (int16_t sample : buffer) {
      if (sample != 0) {
        anyNonZero = true;
        break;
      }
    }
  }
  assert(anyNonZero);
  // 声部播完后输出全零。
  audio.fillBuffer(buffer.data(), static_cast<int32_t>(buffer.size()));
  for (int16_t sample : buffer) assert(sample == 0);
}

// 合成器：每种音效生成预期长度的非静音 PCM。
void testSynthesis() {
  const SoundEffect all[] = {
      SoundEffect::Hit,      SoundEffect::HeavyHit,
      SoundEffect::Dodge,    SoundEffect::Kill,
      SoundEffect::PoiseBreak, SoundEffect::CastBarBroken,
      SoundEffect::Resonance, SoundEffect::AuraApplied,
      SoundEffect::PhaseChanged, SoundEffect::Ambient};
  for (SoundEffect effect : all) {
    const std::vector<int16_t> pcm = synthesizeSound(effect);
    assert(pcm.size() > 1000);  // 均 >22ms
    bool anyNonZero = false;
    for (int16_t sample : pcm) {
      if (sample != 0) {
        anyNonZero = true;
        break;
      }
    }
    assert(anyNonZero);
    // 确定性：同音效两次合成结果一致。
    assert(synthesizeSound(effect) == pcm);
  }
}

// 声部抢占：超过声部数的并发音效不崩溃且仍可混音。
void testVoiceStealing() {
  AudioBridge audio;
  CombatEventBatch batch{};
  for (int i = 0; i < AudioBridge::kVoiceCount + 4; ++i) {
    GameplayEvent damage{};
    damage.type = GameplayEventType::Damage;
    damage.value = fp(8);
    batch.gameplay.push_back(damage);
  }
  audio.dispatch(batch);
  assert(audio.lastDispatched().size() ==
         static_cast<size_t>(AudioBridge::kVoiceCount + 4));
  std::vector<int16_t> buffer(256, 0);
  assert(audio.fillBuffer(buffer.data(), 256) == 512);
}

// 环境音垫：循环播放超过自身长度后仍有输出；停止后归零。
void testAmbientLoop() {
  AudioBridge audio;
  audio.startAmbient();
  assert(audio.ambientPlaying());
  audio.startAmbient();  // 幂等：不重复占声部。
  assert(audio.ambientPlaying());
  std::vector<int16_t> buffer(8192, 0);
  // Ambient 段长 4s = 176400 帧；填充超过一个循环验证回绕。
  bool nonZeroBeyondLoop = false;
  for (int chunk = 0; chunk < 24; ++chunk) {
    audio.fillBuffer(buffer.data(), static_cast<int32_t>(buffer.size()));
    if (chunk >= 22) {
      for (int16_t sample : buffer) {
        if (sample != 0) {
          nonZeroBeyondLoop = true;
          break;
        }
      }
    }
  }
  assert(nonZeroBeyondLoop);
  // 一次性音效与垫底共存：垫底不被抢占。
  CombatEventBatch batch{};
  GameplayEvent damage{};
  damage.type = GameplayEventType::Damage;
  damage.value = fp(8);
  batch.gameplay.push_back(damage);
  audio.dispatch(batch);
  assert(audio.ambientPlaying());
  audio.stopAmbient();
  assert(!audio.ambientPlaying());
  // 一次性 Hit 播完后（0.07s），整体归静。
  for (int chunk = 0; chunk < 6; ++chunk) {
    audio.fillBuffer(buffer.data(), static_cast<int32_t>(buffer.size()));
  }
  audio.fillBuffer(buffer.data(), static_cast<int32_t>(buffer.size()));
  for (int16_t sample : buffer) assert(sample == 0);
}

}  // namespace

int main() {
  testDispatchEmptyBatchDoesNotCrash();
  testDispatchWithEventsDoesNotCrash();
  testStartStopDoesNotCrash();
  testMultipleDispatchesDoesNotCrash();
  testEventMapping();
  testHeavyDamageUpgrade();
  testMixingProducesThenSilence();
  testSynthesis();
  testVoiceStealing();
  testAmbientLoop();
  return 0;
}
