#pragma once

#include <cstdlib>
#include <iostream>
#include "glad/glad.h"

#include <GLFW/glfw3.h>
#include <plog/Log.h>
#include "plog/Initializers/RollingFileInitializer.h"

#ifndef NDEBUG

#include "plog/Initializers/ConsoleInitializer.h"
#include "plog/Formatters/FuncMessageFormatter.h"

#endif

#include "imgui.h"
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <chrono>
#include "Texture.h"

namespace Engine {
    void error_callback(int code, const char *message);

    class Window {
    private:
        GLFWwindow *window;
        glm::vec<2, double> cursor{};
        bool firstUpdate = true;
    public:
        int width, height;

        static void init();
        static void destroy();

        explicit Window(int w = 1280, int h = 720, const char *title = "Untitled");

        ~Window();

        static void setVsync(bool value);

        void setTitle(const char *title);

        void reset() const;

        GLFWwindow *getId();

        bool update(bool *resized);

        void hideCursor();

        void showCursor();

        void setCursorPosition(glm::vec2 position);

        glm::vec2 getCursorPosition();

        bool isKeyPressed(int key);

        void setIcon(const char *path);
    };
}
