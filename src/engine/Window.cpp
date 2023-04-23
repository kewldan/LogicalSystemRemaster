#include "Window.h"


void Engine::error_callback(int code, const char* message) {
	PLOGE << code << " | " << message;
}

Engine::Window::Window(int w, int h, const char* title, bool notInitGlfw) {
	std::remove("latest.log");
	plog::init(plog::debug, "latest.log");
#ifndef NDEBUG
	plog::get()->addAppender(new plog::ColorConsoleAppender<plog::FuncMessageFormatter>());
#endif
	PLOGI << "<< LOADING LIBRARIES >>";
	PLOGI << "ImGui version: " << ImGui::GetVersion();
	PLOGI << "Glfw version: " << glfwGetVersionString();

	if (!notInitGlfw) {
		PLOG_INFO << "[INTERNAL] GLFW initialized";
		glfwSetErrorCallback(Engine::error_callback);
		if (!glfwInit())
			exit(EXIT_FAILURE);
	}

	width = w;
	height = h;
	cursor = glm::vec<2, double>(0);

	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	window = glfwCreateWindow(w, h, title, nullptr, nullptr);
	if (!window) {
		PLOG_FATAL << "[INTERNAL] Window can not be initialized";
		glfwTerminate();
		exit(EXIT_FAILURE);
	}
	glfwMakeContextCurrent(window);

	if (!notInitGlfw) {
		gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
		PLOGI << "[INTERNAL] GLAD loaded";
	}

	const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
	PLOGI << "Renderer: " << renderer;

	const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
	PLOGI << "OpenGL: " << version;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_MULTISAMPLE);
}

void Engine::Window::setTitle(const char* title)
{
	glfwSetWindowTitle(window, title);
}

static void error_callback(int error, const char* description) {
	PLOG_ERROR << "[GLFW] " << error << " | " << description;
}

GLFWwindow* Engine::Window::getId() {
	return window;
}

Engine::Window::~Window() {
	PLOGW << "W";
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Engine::Window::setVsync(bool value)
{
	glfwSwapInterval(value);
}

bool Engine::Window::update(bool* resized) {
	glfwSwapBuffers(window);
	glfwPollEvents();

	int lw = width, lh = height;
	glfwGetFramebufferSize(window, &width, &height);
	*resized = (lw != width || lh != height);

	glfwGetCursorPos(window, &cursor.x, &cursor.y);

	return !glfwWindowShouldClose(window);
}

void Engine::Window::hideCursor() {
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Engine::Window::showCursor() {
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}


void Engine::Window::setCursorPosition(glm::vec2 position) {
	glfwSetCursorPos(window, position.x, position.y);
}

glm::vec2 Engine::Window::getCursorPosition() {
	return static_cast<glm::vec2>(cursor);
}

bool Engine::Window::isKeyPressed(int key) {
	return glfwGetKey(window, key) == 1;
}

void Engine::Window::reset() const {
	glViewport(0, 0, width, height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Engine::Window::setIcon(const char* path) {
	GLFWimage image{};
	image.pixels = Engine::Texture::loadImage(path, &image.width, &image.height);
	if (image.pixels) {
		glfwSetWindowIcon(window, 1, &image);
	}
	stbi_image_free(image.pixels);
}