include(FetchContent)

# Some fetched deps still declare cmake_minimum_required(VERSION 3.0), which
# modern CMake rejects; treat those old minimums as 3.5.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

add_link_options(
        -sUSE_GLFW=3
        -sUSE_ZLIB=1
        -sMIN_WEBGL_VERSION=2
        -sMAX_WEBGL_VERSION=2
        -sFULL_ES3=1
        -sALLOW_MEMORY_GROWTH=1
        -sEXPORTED_RUNTIME_METHODS=UTF8ToString
        -sSTACK_SIZE=1048576
)

FetchContent_Declare(glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE)
FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_Declare(plog
        GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
        GIT_TAG 1.1.10
        GIT_SHALLOW TRUE)
FetchContent_Declare(unordered_dense
        GIT_REPOSITORY https://github.com/martinus/unordered_dense.git
        GIT_TAG v4.4.0
        GIT_SHALLOW TRUE)
FetchContent_Declare(stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
        GIT_SHALLOW TRUE)
FetchContent_Declare(imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.8
        GIT_SHALLOW TRUE)
FetchContent_Declare(base64
        GIT_REPOSITORY https://github.com/aklomp/base64.git
        GIT_TAG v0.5.2
        GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(glm nlohmann_json plog unordered_dense stb imgui base64)

set(Stb_INCLUDE_DIR "${stb_SOURCE_DIR}" CACHE PATH "" FORCE)

add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
add_library(imgui::imgui ALIAS imgui)

if (NOT TARGET aklomp::base64)
    add_library(aklomp::base64 ALIAS base64)
endif ()

add_library(ls_glfw_stub INTERFACE)
add_library(glfw ALIAS ls_glfw_stub)

add_library(ZLIB::ZLIB INTERFACE IMPORTED)
add_library(glad::glad INTERFACE IMPORTED)
add_library(nfd::nfd INTERFACE IMPORTED)
