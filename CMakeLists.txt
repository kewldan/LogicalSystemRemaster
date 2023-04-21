cmake_minimum_required (VERSION 3.23)
project ("LogicalSystem")

add_definitions(-DBLOCK_BATCHING=2048)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBRARIES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libraries)
include(${LIBRARIES_PATH}/imgui/import.cmake)

include_directories(
	"${LIBRARIES_PATH}/glad/include"
	"${LIBRARIES_PATH}/stb/include"
	"${LIBRARIES_PATH}/plog/include"
	"${LIBRARIES_PATH}/glfw/include"
	"${LIBRARIES_PATH}/glm/include"
	"${LIBRARIES_PATH}/imgui"
	"${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/include"
	"${PROJECT_SOURCE_DIR}/src/engine"
    "${PROJECT_SOURCE_DIR}/include/engine"
)

link_directories("${CMAKE_CURRENT_SOURCE_DIR}/libraries/glfw/lib-vc2022")

set(ENGINE_SRCS
	"include/engine/Camera.h" "src/engine/Camera.cpp"
	"include/engine/File.h" "src/engine/File.cpp"
	"include/engine/Shader.h" "src/engine/Shader.cpp"
	"include/engine/Texture.h" "src/engine/Texture.cpp"
	"include/engine/Window.h" "src/engine/Window.cpp"
)

set(PROJECT_SRCS
        "include/Block.h" "src/Block.cpp"
		"include/BlockManager.h" "src/BlockManager.cpp"
		"include/HUD.h" "src/HUD.cpp"
		"include/main.h" "src/main.cpp"
		"include/RenderPipeline.h" "src/RenderPipeline.cpp"
)

add_executable (${PROJECT_NAME} ${ENGINE_SRCS} ${PROJECT_SRCS} ${IMGUI_SOURCES} "${LIBRARIES_PATH}/glad/src/glad.c")

target_link_libraries(${PROJECT_NAME} "glfw3" "opengl32" "gdi32")
