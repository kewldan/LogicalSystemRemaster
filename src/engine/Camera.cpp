#include "Camera.h"

Engine::Camera::Camera(Engine::Window* window) {
	position = glm::vec3(0);
	view = glm::mat4(1);
	orthographic = glm::mat4(1);
	smoothZoomStart = 0;

	this->window = window;
}

glm::mat4 Engine::Camera::getView() {
	return view;
}

glm::mat4 Engine::Camera::getOrthographic() {
	return orthographic;
}

void Engine::Camera::update()
{
	view = glm::mat4(1);
	view = glm::translate(view, -position);
	orthographic = glm::ortho(window->width * (1 - zoom), window->width * zoom, window->height * (1 - zoom), window->height * zoom, 0.1f, 10.0f);
	if (glfwGetTime() < smoothZoomStart + 0.5) {
		zoom = std::lerp(zoom, targetZoom, (float) (glfwGetTime() - smoothZoomStart) / 0.5f);
	}
	else {
		zoom = targetZoom;
	}
}

void Engine::Camera::zoomIn(float factor)
{
	targetZoom += factor;
	targetZoom = std::max(std::min(targetZoom, 3.f), 0.6f);
	smoothZoomStart = glfwGetTime();
}
