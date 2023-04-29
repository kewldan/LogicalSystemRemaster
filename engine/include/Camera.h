#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "Window.h"
#include <cmath>

namespace Engine {
    class Camera {
    private:
        glm::mat4 view{}, orthographic{};
        float targetZoom = 1.f;
        bool zoomChanging;
        double smoothZoomStart;
    public:
        Window *window;
        float zoom = 1.f;
        float left{}, right{}, top{}, bottom{};
        glm::vec3 position{};

        explicit Camera(Window *window);

        glm::mat4 getView();

        glm::mat4 getOrthographic();

        void update();

        void updateView();

        void updateOrthographic();

        void zoomIn(float factor);

        void setZoom(float newZoom);
    };
}
