cmake_minimum_required(VERSION 3.23)
project("LogicalSystem")

add_definitions(-DBLOCK_BATCHING=8192)

set(CMAKE_CXX_STANDARD 20)
set(LIBRARIES_PATH "E:/Projects/libraries")
set(GLAD_PATH "${LIBRARIES_PATH}/glad/release")
include(${LIBRARIES_PATH}/imgui/import.cmake)

include_directories(
        "${GLAD_PATH}/include"
        "${LIBRARIES_PATH}/stb/include"
        "${LIBRARIES_PATH}/b64"
        "${LIBRARIES_PATH}/plog/include"
        "${LIBRARIES_PATH}/freetype/include"
        "${LIBRARIES_PATH}/glfw/include"
        "${LIBRARIES_PATH}/glm/include"
        "${LIBRARIES_PATH}/zlib"
        "${LIBRARIES_PATH}/bson"
        "${LIBRARIES_PATH}/imgui"
        "${PROJECT_SOURCE_DIR}/include"
        "${LIBRARIES_PATH}/engine/include"
)

link_directories("${LIBRARIES_PATH}/glfw/lib-mingw-w64")
link_directories("${LIBRARIES_PATH}/zlib/lib")
link_directories("${LIBRARIES_PATH}/freetype/lib")

file(GLOB PROJECT_SRCS
        "include/*.h" "src/*.cpp"
        )

file(GLOB ENGINE_SRCS
        "${LIBRARIES_PATH}/engine/include/*.h" "${LIBRARIES_PATH}/engine/src/*.cpp"
        "${LIBRARIES_PATH}/engine/include/io/*.h" "${LIBRARIES_PATH}/engine/src/io/*.cpp"
        )

set(LIBRARIES_SRCS
        ${IMGUI_SOURCES}
        "${GLAD_PATH}/src/glad.c"
        "${LIBRARIES_PATH}/b64/Base64.cpp"
        )

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")

add_executable(${PROJECT_NAME} ${PROJECT_SRCS} ${ENGINE_SRCS} ${LIBRARIES_SRCS} "${CMAKE_CURRENT_SOURCE_DIR}/resources.rc")

target_link_libraries(${PROJECT_NAME}
        debug freetyped
        optimized freetype)

target_link_libraries(${PROJECT_NAME}
        debug zlibstaticd
        optimized zlibstatic)

target_link_libraries(${PROJECT_NAME} "glfw3" "opengl32")
