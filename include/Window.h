#pragma once

#include <cstdlib>
#include <iostream>
#include "glad/glad.h"

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <plog/Log.h>
#include "plog/Initializers/RollingFileInitializer.h"
#include "plog/Formatters/FuncMessageFormatter.h"
#include "plog/Appenders/ColorConsoleAppender.h"
#include "imgui.h"
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <chrono>

class Window {
private:
	static void initializeGlfw();

	GLFWwindow* window;
	glm::vec<2, double> cursor;
public:
	int width, height;
	Window(int w = 1280, int h = 720, const char* title = "Untitled", bool notInitGlfw = false);

	~Window();

	void setVsync(bool value);

	void setTitle(const char* title);

	void reset() const;

	GLFWwindow* getId();

	bool update(bool* resized);

	void hideCursor();

	void showCursor();

	void setCursorPosition(int x, int y);

	glm::vec2 getCursorPosition();

	bool isKeyPressed(int key);

	static void debugDraw(bool value);
};
