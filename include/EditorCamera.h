#pragma once

#include "SmoothZoom.h"

#include <Window.h>
#include <glm/ext.hpp>

class EditorCamera {
public:
    explicit EditorCamera(Engine::Window *window);

    void update();

    [[nodiscard]] float getZoom() const;

    void setZoom(float zoom);

    // Accepts the raw GLFW vertical scroll delta. The anchor uses normalized
    // framebuffer coordinates: x from the left, y from the bottom.
    void addZoomInput(float scrollDelta, glm::vec2 anchor);

    void frameWorldBounds(float minX, float minY, float maxX, float maxY);

    [[nodiscard]] const glm::mat4 &getView() const;

    [[nodiscard]] const glm::mat4 &getProjection() const;

    Engine::Window *window;
    float left{}, right{}, top{}, bottom{};
    glm::vec3 position{};

private:
    glm::mat4 view{}, orthographic{};
    SmoothZoom zoom;
    glm::vec2 zoomAnchor{0.5f, 0.5f};
    float lastZoom{1.f};
    double lastUpdateTime{};
    float zNear = 0.01f, zFar = 10.f;
};
