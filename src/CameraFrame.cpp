#include "CameraFrame.h"
#include "SmoothZoom.h"

#include <algorithm>

CameraFrame calculateCameraFrame(float minX, float minY, float maxX, float maxY,
                                 int viewportWidth, int viewportHeight, float padding) {
    const float availableWidth = std::max(1.f, static_cast<float>(viewportWidth) - padding * 2.f);
    const float availableHeight = std::max(1.f, static_cast<float>(viewportHeight) - padding * 2.f);
    const float contentWidth = std::max(32.f, maxX - minX);
    const float contentHeight = std::max(32.f, maxY - minY);
    const float viewScale = std::max(contentWidth / availableWidth, contentHeight / availableHeight);
    const float zoom = std::clamp((viewScale + 1.f) * 0.5f, SmoothZoom::MIN_ZOOM, SmoothZoom::MAX_ZOOM);

    return {
            (minX + maxX) * 0.5f - static_cast<float>(viewportWidth) * 0.5f,
            (minY + maxY) * 0.5f - static_cast<float>(viewportHeight) * 0.5f,
            zoom
    };
}
