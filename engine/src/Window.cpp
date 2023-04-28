#include "Window.h"

Engine::Window::Window(int w, int h, const char *title) {
    width = w;
    height = h;

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

    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    PLOGI << "[INTERNAL] GLAD loaded";

    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    PLOGI << "Renderer: " << renderer;

    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    PLOGI << "OpenGL: " << version;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_MULTISAMPLE);
}

void Engine::Window::setTitle(const char *title) {
    glfwSetWindowTitle(window, title);
}

void Engine::error_callback(int error, const char *description) {
    PLOGF << "[GLFW] " << error << " | " << description;
}

GLFWwindow *Engine::Window::getId() {
    return window;
}

Engine::Window::~Window() {
    glfwDestroyWindow(window);
}

void Engine::Window::setVsync(bool value) {
    glfwSwapInterval(value);
}

bool Engine::Window::update() {
    glfwSwapBuffers(window);
    glfwPollEvents();

    int lw = width, lh = height;
    glfwGetFramebufferSize(window, &width, &height);
    resized = (lw != width || lh != height);

    return !glfwWindowShouldClose(window);
}

void Engine::Window::reset() const {
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Engine::Window::setIcon(const char *path) {
    GLFWimage image{};
    image.pixels = Engine::Texture::loadImage(path, &image.width, &image.height);
    if (image.pixels) {
        glfwSetWindowIcon(window, 1, &image);
    }
    stbi_image_free(image.pixels);
}

void Engine::Window::init() {
    std::remove("latest.log");
    plog::init(plog::debug, "latest.log");
#ifndef NDEBUG
    plog::get()->addAppender(new plog::ColorConsoleAppender<plog::FuncMessageFormatter>());
#endif
    PLOGI << "<< LOADING LIBRARIES >>";
    PLOGI << "ImGui version: " << ImGui::GetVersion();
    PLOGI << "Glfw version: " << glfwGetVersionString();
    PLOGI << "Build: " << __TIME__ << " " << __DATE__;

    PLOGI << "[INTERNAL] GLFW initialized";
    glfwSetErrorCallback(Engine::error_callback);
    if (!glfwInit())
        exit(EXIT_FAILURE);
}

void Engine::Window::destroy() {
    glfwTerminate();
}

bool Engine::Window::isResized() {
    if(resized) {
        resized = false;
        return true;
    }else{
        return false;
    }
}
