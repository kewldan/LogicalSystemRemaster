#pragma once

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui_notify.h"
#include "imfilebrowser.h"
#include "Window.h"

namespace Engine {
    class HUD {
    public:
        static void init(Engine::Window *window);

        static void destroy();

        static void begin();

        static void end();
    };
}
