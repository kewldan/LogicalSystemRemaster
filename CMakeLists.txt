cmake_minimum_required(VERSION 3.23)
project("LogicalSystem")

add_definitions(-DBLOCK_BATCHING=2048)

set(CMAKE_CXX_STANDARD 20)
set(LIBRARIES_PATH ${CMAKE_CURRENT_SOURCE_DIR}/libraries)
set(GLAD_PATH "${LIBRARIES_PATH}/glad/release")

set(IMGUI_SOURCES
        ${LIBRARIES_PATH}/imgui/imconfig.h
        ${LIBRARIES_PATH}/imgui/imgui_draw.cpp
        ${LIBRARIES_PATH}/imgui/imgui_internal.h
        ${LIBRARIES_PATH}/imgui/imgui_tables.cpp
        ${LIBRARIES_PATH}/imgui/imgui_widgets.cpp
        ${LIBRARIES_PATH}/imgui/backends/imgui_impl_glfw.cpp ${LIBRARIES_PATH}/imgui/backends/imgui_impl_glfw.h
        ${LIBRARIES_PATH}/imgui/backends/imgui_impl_opengl3.cpp ${LIBRARIES_PATH}/imgui/backends/imgui_impl_opengl3.h
        ${LIBRARIES_PATH}/imgui/imgui.cpp ${LIBRARIES_PATH}/imgui/imgui.h
        )

include_directories(
        "${GLAD_PATH}/include"
        "${LIBRARIES_PATH}/stb/include"
        "${LIBRARIES_PATH}/b64"
        "${LIBRARIES_PATH}/plog/include"
        "${LIBRARIES_PATH}/glfw/include"
        "${LIBRARIES_PATH}/glm/include"
        "${LIBRARIES_PATH}/glm/include"
        "${LIBRARIES_PATH}/zlib"
        "${LIBRARIES_PATH}/imgui"
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/engine/include"
)

link_directories("${LIBRARIES_PATH}/glfw/lib-vc2022")
link_directories("${LIBRARIES_PATH}/zlib/lib")

file(GLOB ENGINE_SRCS
        "engine/include/*.h" "engine/src/*.cpp"
        )

file(GLOB PROJECT_SRCS
        "include/*.h" "src/*.cpp"
        )

set(LIBRARIES_SRCS
        ${IMGUI_SOURCES}
        "${GLAD_PATH}/src/glad.c"
        "${LIBRARIES_PATH}/b64/Base64.cpp")

add_executable(${PROJECT_NAME} ${ENGINE_SRCS} ${PROJECT_SRCS} ${LIBRARIES_SRCS} "${CMAKE_CURRENT_SOURCE_DIR}/resources.rc")

add_custom_command(TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(${PROJECT_NAME} PROPERTIES
            LINK_FLAGS "/ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS")
endif ()

target_link_libraries(${PROJECT_NAME} "glfw3" "opengl32" "zlibstaticd")
