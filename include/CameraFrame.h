#pragma once

struct CameraFrame {
    float positionX{};
    float positionY{};
    float zoom{1.f};
};

[[nodiscard]] CameraFrame calculateCameraFrame(float minX, float minY, float maxX, float maxY,
                                               int viewportWidth, int viewportHeight,
                                               float padding = 72.f);
