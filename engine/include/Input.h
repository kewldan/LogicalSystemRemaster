#pragma once

#include "glm/ext.hpp"
#include <GLFW/glfw3.h>

namespace Engine {
    class Input {
    private:
        GLFWwindow* window;
        glm::vec2 cursorPosition{};
        bool* keyPressed, *mousePressed;
        bool* keyJustPressed, *mouseJustPressed;
    public:
        explicit Input(GLFWwindow *window);

        void hideCursor();

        void showCursor();

        glm::vec2 getCursorPosition();

        void setCursorPosition(glm::vec2 position);

        void update();

        bool isKeyPressed(int key);

        bool isKeyJustPressed(int key);

        bool isMouseButtonPressed(int button);

        bool isMouseButtonJustPressed(int button);
    };
}