#include "native/engine/world/weather_system.h"

#include <algorithm>
#include <cmath>

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

EnvironmentState WeatherSystem::environmentAt(float gameSeconds, float hour) {
  if (!std::isfinite(hour)) hour = 12.0f;
  hour = std::fmod(hour, 24.0f);
  if (hour < 0.0f) hour += 24.0f;
  constexpr float kPi = 3.14159265358979323846f;
  const float solar = 0.5f +
                      0.5f * std::cos((hour - 12.0f) / 24.0f * 2.0f * kPi);
  const float daylight = 0.18f + 0.82f * solar;
  const WeatherState weather = weatherAt(gameSeconds);

  EnvironmentState state;
  state.weatherId = weather.id;
  state.daylight = daylight;
  state.lightColor = glm::vec3(0.82f, 0.80f, 0.72f) *
                     (daylight * weather.lightScale);
  state.ambientColor = glm::vec3(0.18f, 0.21f, 0.28f) *
                       (0.55f + 0.45f * daylight);
  const glm::vec3 nightTop{0.015f, 0.028f, 0.075f};
  const glm::vec3 dayTop{0.20f, 0.42f, 0.72f};
  const glm::vec3 nightHorizon{0.08f, 0.10f, 0.17f};
  const glm::vec3 dayHorizon{0.62f, 0.76f, 0.84f};
  state.skyTop = nightTop * (1.0f - solar) + dayTop * solar;
  state.skyHorizon = nightHorizon * (1.0f - solar) + dayHorizon * solar;
  state.fogColor = state.skyHorizon;
  state.fogDensity = 0.035f + weather.fogDensity * 0.22f;

  switch (weather.id) {
    case kWeatherCloudy:
      state.cloudCoverage = 0.62f;
      state.windStrength = 0.32f;
      state.waterRoughness = 0.38f;
      state.skyTop *= 0.78f;
      break;
    case kWeatherRain:
      state.cloudCoverage = 0.94f;
      state.windStrength = 0.72f;
      state.precipitation = PrecipitationKind::Rain;
      state.precipitationIntensity = 1.0f;
      state.waterRoughness = 0.82f;
      state.skyTop *= 0.58f;
      state.skyHorizon *= 0.70f;
      state.fogColor = state.skyHorizon;
      break;
    case kWeatherSnow:
      state.cloudCoverage = 0.82f;
      state.windStrength = 0.46f;
      state.precipitation = PrecipitationKind::Snow;
      state.precipitationIntensity = 0.82f;
      state.waterRoughness = 0.48f;
      state.skyTop = state.skyTop * 0.72f + glm::vec3(0.12f, 0.15f, 0.20f);
      break;
    default:
      state.cloudCoverage = 0.14f;
      state.windStrength = 0.12f;
      state.waterRoughness = 0.20f;
      break;
  }
  return state;
}
