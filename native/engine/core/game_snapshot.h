#pragma once
#include "tick_clock.h"
#include <cstdint>

struct GameSnapshot {
  Tick tick = 0;
  FixedPoint hp = fp(100);
  FixedPoint poise = fp(100);
  float playerX = 0.5f;
  float playerY = 0.5f;
  float fps = 0.0f;
  bool moving = false;
  int32_t targetId = 0;
  int32_t bossPhase = 0;
  bool rendererReady = false;
  float moveX = 0.0f;
  float moveY = 0.0f;
  float cameraYaw = 0.0f;
  float cameraPitch = 0.0f;
  float targetDist = 0.0f;
  uint8_t comboSegment = 0;
  FixedPoint targetHp = fp(300);
  FixedPoint targetPoise = fp(100);
  FixedPoint stamina = fp(100);
  FixedPoint resonance = 0;
  bool hasInsight = false;
  bool invulnerable = false;
  Tick insightMs = 0;
  Tick pulseWarningMs = 0;
  int32_t lastRejectReason = 0;
  uint8_t currentAction = 0;
  Tick comboWindowMs = 0;
  Tick radianceCooldownMs = 0;
  Tick currentCooldownMs = 0;
  Tick corruptionCooldownMs = 0;
  Tick ultimateWindowMs = 0;
  bool targetPoiseBroken = false;
  bool radianceAttached = false;
  bool currentAttached = false;
  bool corruptionAttached = false;
  bool corroded = false;
  int32_t currentReaction = -1;
  uint8_t pulsePhase = 0;
};
