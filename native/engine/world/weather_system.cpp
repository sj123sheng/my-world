#include "native/engine/world/weather_system.h"

namespace {

// 固定槽位序列：晴→多云→雨→多云→晴→雪→多云→晴。
constexpr int32_t kSlots[WeatherSystem::kSlotCount] = {
    WeatherSystem::kWeatherClear, WeatherSystem::kWeatherCloudy,
    WeatherSystem::kWeatherRain,  WeatherSystem::kWeatherCloudy,
    WeatherSystem::kWeatherClear, WeatherSystem::kWeatherSnow,
    WeatherSystem::kWeatherCloudy, WeatherSystem::kWeatherClear,
};

}  // namespace

const int32_t* WeatherSystem::slotSequence() { return kSlots; }

WeatherState WeatherSystem::weatherAt(float gameSeconds) {
  // 负值钳到 0，避免取模歧义。
  float seconds = gameSeconds < 0.0f ? 0.0f : gameSeconds;
  const float cycle = kSlotSeconds * static_cast<float>(kSlotCount);
  while (seconds >= cycle) seconds -= cycle;
  const int32_t slot = static_cast<int32_t>(seconds / kSlotSeconds) % kSlotCount;
  const int32_t id = kSlots[slot];
  WeatherState state;
  state.id = id;
  switch (id) {
    case kWeatherCloudy:
      state.lightScale = 0.8f;
      state.fogDensity = 0.15f;
      break;
    case kWeatherRain:
      state.lightScale = 0.6f;
      state.fogDensity = 0.45f;
      break;
    case kWeatherSnow:
      state.lightScale = 0.75f;
      state.fogDensity = 0.6f;
      break;
    default:  // 晴。
      state.lightScale = 1.0f;
      state.fogDensity = 0.0f;
      break;
  }
  return state;
}
