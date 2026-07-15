#pragma once

#include "native/engine/math/vec2.h"

#include <cmath>

class CameraRenderState {
 public:
  CameraRenderState(Vec2 target = {0.5f, 0.5f}, float yaw = 0.0f,
                    float pitch = 0.45f, float distance = 0.35f,
                    float neutralPitch = 0.45f,
                    float neutralDistance = 0.35f)
      : target_(target),
        yaw_(yaw),
        pitch_(pitch),
        distance_(distance),
        neutralPitch_(neutralPitch),
        neutralDistance_(neutralDistance) {}

  Vec2 target() const { return target_; }
  float yaw() const { return yaw_; }
  float pitch() const { return pitch_; }
  float distance() const { return distance_; }

  Vec2 worldVectorToView(Vec2 worldVector) const {
    const float cosine = std::cos(yaw_);
    const float sine = std::sin(yaw_);
    const float zoom = neutralDistance_ / distance_;
    const float pitchScale = std::cos(pitch_) / std::cos(neutralPitch_);
    const float rotatedX = cosine * worldVector.x - sine * worldVector.y;
    const float rotatedY = sine * worldVector.x + cosine * worldVector.y;
    return {rotatedX * zoom, rotatedY * zoom * pitchScale};
  }

  Vec2 worldToView(Vec2 world) const {
    const Vec2 viewOffset = worldVectorToView(world - target_);
    return {0.5f + viewOffset.x, 0.5f + viewOffset.y};
  }

  Vec2 worldSizeToView(Vec2 worldSize) const {
    const float zoom = neutralDistance_ / distance_;
    const float pitchScale = std::cos(pitch_) / std::cos(neutralPitch_);
    return {worldSize.x * zoom, worldSize.y * zoom * pitchScale};
  }

  Vec2 billboardNdcRadii(float worldRadius, float viewportAspect) const {
    // Screen-facing billboards keep equal pixel radii. Pitch affects their
    // world-space center but not their on-screen shape.
    const float zoom = neutralDistance_ / distance_;
    return {worldRadius * zoom * viewportAspect, worldRadius * zoom};
  }

 private:
  Vec2 target_;
  float yaw_;
  float pitch_;
  float distance_;
  float neutralPitch_;
  float neutralDistance_;
};
