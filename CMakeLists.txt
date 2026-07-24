cmake_minimum_required(VERSION 3.25)
project(LogicalSystem
        HOMEPAGE_URL https://kewldan.itch.io/logical-system
        VERSION 2.0.9
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)

find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(plog CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Stb REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(base64 CONFIG REQUIRED)
find_package(unofficial-nativefiledialog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Engine: use a local checkout if present, otherwise fetch it from GitHub
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Engine" CACHE PATH "Path to a kewldan/Engine checkout")
if (EXISTS "${ENGINE_DIR}/CMakeLists.txt")
    add_subdirectory("${ENGINE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/engine")
else ()
    include(FetchContent)
    FetchContent_Declare(Engine
            GIT_REPOSITORY https://github.com/kewldan/Engine.git
            GIT_TAG main
    )
    FetchContent_MakeAvailable(Engine)
endif ()

file(GLOB_RECURSE PROJECT_SRCS
        src/*.cpp
)

add_executable(LogicalSystem WIN32 ${PROJECT_SRCS} resources.rc)

target_compile_definitions(LogicalSystem PRIVATE BLOCK_BATCHING=8192)
target_include_directories(LogicalSystem PRIVATE include ${Stb_INCLUDE_DIR})
target_link_libraries(LogicalSystem PRIVATE Engine glad::glad glfw glm::glm plog::plog ZLIB::ZLIB aklomp::base64 imgui::imgui unofficial::nativefiledialog::nfd)
