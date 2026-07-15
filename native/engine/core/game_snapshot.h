#pragma once
#include "tick_clock.h"
#include <cstdint>

struct GameSnapshot {
  Tick tick = 0;
  int32_t hp = 100;
  int32_t poise = 100;
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
  FixedPoint playerHp = fp(100);
  FixedPoint targetHp = fp(300);
  FixedPoint targetPoise = fp(100);
  FixedPoint stamina = fp(100);
  FixedPoint resonance = 0;
  bool hasInsight = false;
};
