cmake_minimum_required(VERSION 3.23)
project(LogicalSystem
        HOMEPAGE_URL https://kewldan.itch.io/logical-system
        VERSION 2.0.9
        LANGUAGES CXX
)

set(ENGINE_DIR "E:\\Projects\\Engine")

add_definitions(-DBLOCK_BATCHING=8192)

set(CMAKE_CXX_STANDARD 20)

include_directories(
        include
        ${ENGINE_DIR}/include
        ${ENGINE_DIR}/thirdparty
)

find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Stb REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(base64 CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)

file(GLOB_RECURSE PROJECT_SRCS
        src/*.cpp
)

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    link_directories(
            ${ENGINE_DIR}/build/release
    )
else ()
    link_directories(
            ${ENGINE_DIR}/build/debug
    )
endif ()

add_executable(LogicalSystem WIN32 ${PROJECT_SRCS} resources.rc)

target_link_libraries(LogicalSystem PRIVATE Engine glad::glad glfw glm::glm plog::plog ZLIB::ZLIB aklomp::base64 imgui::imgui unofficial::nativefiledialog::nfd)
target_include_directories(LogicalSystem PRIVATE ${Stb_INCLUDE_DIR})
