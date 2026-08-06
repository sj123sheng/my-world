#pragma once

#include <cstdint>

// 天气状态：由游戏时钟确定性推导，无随机源，可独立测试。
// id: 0=晴 1=多云 2=雨 3=雪；lightScale 为光照衰减系数，fogDensity 为雾浓度。
struct WeatherState {
  int32_t id = 0;
  float lightScale = 1.0f;
  float fogDensity = 0.0f;
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

  // 槽位序列（暴露供测试与配置校验）。
  static const int32_t* slotSequence();
};
