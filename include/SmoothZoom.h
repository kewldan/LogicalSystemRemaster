#pragma once

// Frame-rate independent zoom model. It works in logarithmic view scale so a
// wheel step changes the visible area by the same proportion at every zoom.
class SmoothZoom {
public:
    static constexpr float MIN_ZOOM = 0.65f;
    static constexpr float MAX_ZOOM = 3.5f;

    explicit SmoothZoom(float zoom = 1.f);

    // Positive scroll zooms in, negative scroll zooms out. Fractional values
    // from precision touchpads naturally add up to the same motion as a wheel.
    void addScroll(float delta);

    void setTarget(float zoom);

    // Advance the critically damped animation and return the displayed zoom.
    float step(float deltaTime);

    [[nodiscard]] float value() const;

    [[nodiscard]] float target() const;

    [[nodiscard]] bool isSettled() const;

private:
    float currentLogScale{};
    float targetLogScale{};
    float velocity{};
};
