#pragma once
#include <cmath>

struct Vec2 {
  float x = 0.0f;
  float y = 0.0f;
  bool operator==(const Vec2& other) const {
    return std::abs(x - other.x) < 0.0001f &&
           std::abs(y - other.y) < 0.0001f;
  }
  Vec2 operator+(Vec2 other) const { return {x + other.x, y + other.y}; }
  Vec2 operator-(Vec2 other) const { return {x - other.x, y - other.y}; }
  Vec2 operator*(float scale) const { return {x * scale, y * scale}; }
  float length() const { return std::sqrt(x * x + y * y); }
  bool finite() const { return std::isfinite(x) && std::isfinite(y); }
};

inline Vec2 ClampLength(Vec2 value, float maximum) {
  const float length = value.length();
  return length > maximum && length > 0.0f
             ? value * (maximum / length)
             : value;
}
