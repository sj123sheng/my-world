#pragma once

#include <cstdint>
#include <glm/vec3.hpp>

// 天气状态：由游戏时钟确定性推导，无随机源，可独立测试。
// id: 0=晴 1=多云 2=雨 3=雪；lightScale 为光照衰减系数，fogDensity 为雾浓度。
struct WeatherState {
  int32_t id = 0;
  float lightScale = 1.0f;
  float fogDensity = 0.0f;
};

enum class PrecipitationKind : int32_t { None = 0, Rain = 1, Snow = 2 };

// 天气与昼夜共同求出的完整环境状态。渲染层只消费这一份状态，避免天空、
// 雾、水面和降水各自重复判断天气 ID。
struct EnvironmentState {
  int32_t weatherId = 0;
  float daylight = 1.0f;
  glm::vec3 lightColor{0.8f, 0.8f, 0.75f};
  glm::vec3 ambientColor{0.25f, 0.25f, 0.30f};
  glm::vec3 skyTop{0.16f, 0.30f, 0.54f};
  glm::vec3 skyHorizon{0.52f, 0.66f, 0.76f};
  glm::vec3 fogColor{0.52f, 0.66f, 0.76f};
  float fogDensity = 0.0f;
  float cloudCoverage = 0.1f;
  float windStrength = 0.1f;
  PrecipitationKind precipitation = PrecipitationKind::None;
  float precipitationIntensity = 0.0f;
  float waterRoughness = 0.2f;
};

// 天气系统：每 kSlotSeconds 游戏秒切换一个天气槽位，
// 槽位序列固定循环（晴→多云→雨→多云→晴→雪→多云→晴），
// 同一时钟输入永远得到同一结果（回放/存档安全）。
class WeatherSystem {
 public:
  static constexpr float kSlotSeconds = 60.0f;
  static constexpr int32_t kSlotCount = 8;
  static constexpr int32_t kWeatherClear = 0;
  static constexpr int32_t kWeatherCloudy = 1;
  static constexpr int32_t kWeatherRain = 2;
  static constexpr int32_t kWeatherSnow = 3;

  // 按累计游戏秒数推导当前天气（纯函数）。
  static WeatherState weatherAt(float gameSeconds);

  // gameSeconds 决定确定性天气槽，hour 决定昼夜（任意输入会钳制/循环）。
  static EnvironmentState environmentAt(float gameSeconds, float hour);

  // 槽位序列（暴露供测试与配置校验）。
  static const int32_t* slotSequence();
};
