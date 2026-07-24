#include "SmoothZoom.h"

#include <algorithm>
#include <cmath>

namespace {
// Camera zoom is stored as half of (view scale + 1) for historical reasons.
float zoomToLogScale(float zoom) {
    return std::log(2.f * zoom - 1.f);
}

float logScaleToZoom(float logScale) {
    return (std::exp(logScale) + 1.f) * 0.5f;
}

constexpr float SCROLL_SENSITIVITY = 0.16f;
constexpr float SPRING_FREQUENCY = 18.f;
constexpr float SETTLED_DISTANCE = 0.0001f;
constexpr float SETTLED_VELOCITY = 0.001f;
}

SmoothZoom::SmoothZoom(float zoom) {
    currentLogScale = zoomToLogScale(std::clamp(zoom, MIN_ZOOM, MAX_ZOOM));
    targetLogScale = currentLogScale;
}

void SmoothZoom::addScroll(float delta) {
    if (!std::isfinite(delta)) return;

    // Avoid an extreme jump after a stalled frame while preserving ordinary
    // wheel ticks and the small accumulated deltas produced by touchpads.
    delta = std::clamp(delta, -8.f, 8.f);
    targetLogScale = std::clamp(
            targetLogScale - delta * SCROLL_SENSITIVITY,
            zoomToLogScale(MIN_ZOOM),
            zoomToLogScale(MAX_ZOOM));
}

void SmoothZoom::setTarget(float zoom) {
    if (!std::isfinite(zoom)) zoom = 1.f;
    targetLogScale = zoomToLogScale(std::clamp(zoom, MIN_ZOOM, MAX_ZOOM));
    velocity = 0.f;
}

float SmoothZoom::step(float deltaTime) {
    // Exact integration of a critically damped spring. Unlike a frame-based
    // lerp, this produces the same result at 30, 60, or 144 Hz.
    const float dt = std::clamp(deltaTime, 0.f, 0.1f);
    const float displacement = currentLogScale - targetLogScale;
    const float decay = std::exp(-SPRING_FREQUENCY * dt);
    const float springStep = (velocity + SPRING_FREQUENCY * displacement) * dt;
    currentLogScale = targetLogScale + (displacement + springStep) * decay;
    velocity = (velocity - SPRING_FREQUENCY * springStep) * decay;

    const float minLogScale = zoomToLogScale(MIN_ZOOM);
    const float maxLogScale = zoomToLogScale(MAX_ZOOM);
    if (currentLogScale < minLogScale) {
        currentLogScale = minLogScale;
        velocity = std::max(velocity, 0.f);
    } else if (currentLogScale > maxLogScale) {
        currentLogScale = maxLogScale;
        velocity = std::min(velocity, 0.f);
    }

    if (std::abs(currentLogScale - targetLogScale) < SETTLED_DISTANCE &&
        std::abs(velocity) < SETTLED_VELOCITY) {
        currentLogScale = targetLogScale;
        velocity = 0.f;
    }
    return value();
}

float SmoothZoom::value() const {
    return logScaleToZoom(currentLogScale);
}

float SmoothZoom::target() const {
    return logScaleToZoom(targetLogScale);
}

bool SmoothZoom::isSettled() const {
    return currentLogScale == targetLogScale && velocity == 0.f;
}
