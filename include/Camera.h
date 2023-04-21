#pragma once

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "Window.h"

class Camera {
	glm::mat4 view, orthographic;
	Window* window;
	float targetZoom = 1.f;
	double smoothZoomStart;
public:
	float zoom = 1.f;
	glm::vec3 position;
	Camera(Window* window);

	glm::mat4 getView();

	glm::mat4 getOrthographic();

	void update();

	void zoomIn(float factor);
};
