cmake_minimum_required(VERSION 3.23)
project("LogicalSystem")

add_definitions(-DBLOCK_BATCHING=8192)

set(CMAKE_CXX_STANDARD 20)
include("C:/Users/kewldan/Desktop/Engine/Import.cmake")

include_directories(
        include
)

file(GLOB PROJECT_SRCS
        "include/*.h" "src/*.cpp"
        )

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows")

add_executable(${PROJECT_NAME} ${PROJECT_SRCS} ${ENGINE_SRCS} "${CMAKE_CURRENT_SOURCE_DIR}/resources.rc")

target_link_libraries(${PROJECT_NAME} glfw3 glad tb64 brotlicommon-static brotlidec-static brotlienc-static Ws2_32 imgui${_D} freetype${_D} bz2${_D} libpng16${_D} zlib${_D})
