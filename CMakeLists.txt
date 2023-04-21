cmake_minimum_required (VERSION 3.23)
project ("LogicalSystem")

add_definitions(-DBLOCK_BATCHING=2048)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set(LIBRARIES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libraries)
include(${LIBRARIES_PATH}/imgui/import.cmake)

include_directories(
	${LIBRARIES_PATH}/glad/include
	${LIBRARIES_PATH}/stb/include
	${LIBRARIES_PATH}/plog/include
	${LIBRARIES_PATH}/glfw/include
	${LIBRARIES_PATH}/glm/include
	${LIBRARIES_PATH}/imgui
	${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/include
)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/libraries/glfw/lib-vc2022)

file(GLOB SRCS
        "${PROJECT_SOURCE_DIR}/include/*.h"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

add_executable (${PROJECT_NAME} ${SRCS} ${IMGUI_SOURCES} "${LIBRARIES_PATH}/glad/src/glad.c")

target_link_libraries(${PROJECT_NAME} glfw3 opengl32 gdi32)
