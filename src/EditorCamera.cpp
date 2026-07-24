#include "EditorCamera.h"
#include "CameraFrame.h"

#include <algorithm>
#include <GLFW/glfw3.h>

EditorCamera::EditorCamera(Engine::Window *window)
        : window(window), lastUpdateTime(glfwGetTime()) {
    update();
}

void EditorCamera::update() {
    const double now = glfwGetTime();
    const float deltaTime = static_cast<float>(now - lastUpdateTime);
    lastUpdateTime = now;

    const float nextZoom = zoom.step(deltaTime);
    if (nextZoom != lastZoom) {
        // Apply the anchor correction incrementally with the animated zoom.
        // The world point below the cursor therefore never jumps on input.
        position.x += (2.f * zoomAnchor.x - 1.f) *
                      static_cast<float>(window->width) * (lastZoom - nextZoom);
        position.y += (2.f * zoomAnchor.y - 1.f) *
                      static_cast<float>(window->height) * (lastZoom - nextZoom);
        lastZoom = nextZoom;
    }

    view = glm::translate(glm::mat4(1.f), -position);
    left = static_cast<float>(window->width) * (1.f - nextZoom);
    right = static_cast<float>(window->width) * nextZoom;
    top = static_cast<float>(window->height) * nextZoom;
    bottom = static_cast<float>(window->height) * (1.f - nextZoom);
    orthographic = glm::ortho(left, right, bottom, top, zNear, zFar);
}

float EditorCamera::getZoom() const {
    return zoom.value();
}

void EditorCamera::setZoom(float newZoom) {
    zoomAnchor = glm::vec2(0.5f);
    zoom.setTarget(newZoom);
}

void EditorCamera::addZoomInput(float scrollDelta, glm::vec2 anchor) {
    zoomAnchor = glm::clamp(anchor, glm::vec2(0.f), glm::vec2(1.f));
    zoom.addScroll(scrollDelta);
}

void EditorCamera::frameWorldBounds(float minX, float minY, float maxX, float maxY) {
    const CameraFrame frame = calculateCameraFrame(minX, minY, maxX, maxY,
                                                   window->width, window->height);
    position.x = frame.positionX;
    position.y = frame.positionY;
    setZoom(frame.zoom);
}

const glm::mat4 &EditorCamera::getView() const {
    return view;
}

const glm::mat4 &EditorCamera::getProjection() const {
    return orthographic;
}
