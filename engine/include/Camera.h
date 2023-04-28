#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "Window.h"

namespace Engine {
    class Camera {
    private:
        glm::mat4 view, orthographic;
        Window *window;
        float targetZoom = 1.f;
        bool zoomChanging;
        double smoothZoomStart;
    public:
        float zoom = 1.f;
        glm::vec3 position;

        explicit Camera(Window *window);

        glm::mat4 getView();

        glm::mat4 getOrthographic();

        void update();

        void updateView();

        void updateOrthographic();

        void zoomIn(float factor);
    };
}
