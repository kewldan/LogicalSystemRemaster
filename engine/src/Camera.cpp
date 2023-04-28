#include "Camera.h"

Engine::Camera::Camera(Engine::Window *window) {
    this->window = window;
    position = glm::vec3(0);
    updateView();
    updateOrthographic();
    smoothZoomStart = 0;
    zoomChanging = false;
}

glm::mat4 Engine::Camera::getView() {
    return view;
}

glm::mat4 Engine::Camera::getOrthographic() {
    return orthographic;
}

void Engine::Camera::update() {
    if (zoomChanging) {
        if (glfwGetTime() < smoothZoomStart + 0.5) {
            zoom = std::lerp(zoom, targetZoom, (float) (glfwGetTime() - smoothZoomStart) / 0.5f);
        } else {
            zoom = targetZoom;
            zoomChanging = false;
        }
        updateOrthographic();
    }
}

void Engine::Camera::updateView() {
    view = glm::translate(glm::mat4(1), -position);
}

void Engine::Camera::updateOrthographic() {
    left = (float) window->width * (1 - zoom);
    right = (float) window->width * zoom;
    top = (float) window->height * zoom;
    bottom = (float) window->height * (1 - zoom);
    orthographic = glm::ortho(left, right, bottom,
                              top, 0.01f, 10.0f);
}

void Engine::Camera::zoomIn(float factor) {
    targetZoom += factor;
    targetZoom = std::max(std::min(targetZoom, 3.f), 0.6f);
    smoothZoomStart = glfwGetTime();
    zoomChanging = true;
}
