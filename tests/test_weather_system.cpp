#include "native/engine/world/weather_system.h"

#include <cassert>

int main() {
  // 确定性：同一时钟输入永远得到同一结果。
  assert(WeatherSystem::weatherAt(0.0f).id == WeatherSystem::kWeatherClear);
  assert(WeatherSystem::weatherAt(0.0f).id == WeatherSystem::weatherAt(0.0f).id);

  // 每个槽位 60 秒，序列：晴→多云→雨→多云→晴→雪→多云→晴。
  assert(WeatherSystem::weatherAt(30.0f).id == WeatherSystem::kWeatherClear);
  assert(WeatherSystem::weatherAt(60.0f).id == WeatherSystem::kWeatherCloudy);
  assert(WeatherSystem::weatherAt(120.0f).id == WeatherSystem::kWeatherRain);
  assert(WeatherSystem::weatherAt(180.0f).id == WeatherSystem::kWeatherCloudy);
  assert(WeatherSystem::weatherAt(240.0f).id == WeatherSystem::kWeatherClear);
  assert(WeatherSystem::weatherAt(300.0f).id == WeatherSystem::kWeatherSnow);
  assert(WeatherSystem::weatherAt(360.0f).id == WeatherSystem::kWeatherCloudy);
  assert(WeatherSystem::weatherAt(420.0f).id == WeatherSystem::kWeatherClear);

  // 循环：一个完整周期（8 槽 × 60 秒 = 480 秒）后回到起点。
  assert(WeatherSystem::weatherAt(480.0f).id == WeatherSystem::weatherAt(0.0f).id);
  assert(WeatherSystem::weatherAt(1000.0f).id ==
         WeatherSystem::weatherAt(1000.0f - 480.0f).id);

  // 光照衰减：晴天最亮，雨天最暗，雪与多云居中。
  assert(WeatherSystem::weatherAt(0.0f).lightScale >
         WeatherSystem::weatherAt(60.0f).lightScale);
  assert(WeatherSystem::weatherAt(60.0f).lightScale >
         WeatherSystem::weatherAt(120.0f).lightScale);
  assert(WeatherSystem::weatherAt(120.0f).lightScale > 0.0f);

  // 雾浓度：晴天无雾，雨/雪有雾。
  assert(WeatherSystem::weatherAt(0.0f).fogDensity == 0.0f);
  assert(WeatherSystem::weatherAt(120.0f).fogDensity > 0.0f);
  assert(WeatherSystem::weatherAt(300.0f).fogDensity > 0.0f);

  // 负值钳制到 0，不产生歧义。
  assert(WeatherSystem::weatherAt(-5.0f).id == WeatherSystem::weatherAt(0.0f).id);

  // 槽位序列暴露且长度正确。
  const int32_t* seq = WeatherSystem::slotSequence();
  assert(seq[0] == WeatherSystem::kWeatherClear);
  assert(seq[WeatherSystem::kSlotCount - 1] == WeatherSystem::kWeatherClear);

  return 0;
}
