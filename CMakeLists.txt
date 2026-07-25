cmake_minimum_required(VERSION 3.25)
project(LogicalSystem
        HOMEPAGE_URL https://kewldan.itch.io/logical-system
        VERSION 2.3.0
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
find_package(nfd CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)

# Engine: use a local checkout if present, otherwise fetch it from GitHub
set(ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Engine" CACHE PATH "Path to a kewldan/Engine checkout")
if (EXISTS "${ENGINE_DIR}/CMakeLists.txt")
    add_subdirectory("${ENGINE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/engine")
else ()
    include(FetchContent)
    # An already populated checkout remains usable when configuring offline;
    # a missing checkout is still fetched on the first configure.
    set(FETCHCONTENT_UPDATES_DISCONNECTED_ENGINE ON)
    if (APPLE)
        FetchContent_Declare(Engine
                GIT_REPOSITORY https://github.com/kewldan/Engine.git
                GIT_TAG f9e1630cb35e271610ee9bcb475a50b3c45b3e07
                PATCH_COMMAND ${CMAKE_COMMAND}
                -DENGINE_SOURCE_DIR=<SOURCE_DIR>
                -DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/engine-macos.patch
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ApplyEnginePatch.cmake
        )
    else ()
        FetchContent_Declare(Engine
                GIT_REPOSITORY https://github.com/kewldan/Engine.git
                GIT_TAG f9e1630cb35e271610ee9bcb475a50b3c45b3e07
        )
    endif ()
    FetchContent_MakeAvailable(Engine)
endif ()

file(GLOB_RECURSE PROJECT_SRCS
        src/*.cpp
)

if (WIN32)
    add_executable(LogicalSystem WIN32 ${PROJECT_SRCS} resources.rc)
elseif (APPLE)
    # Keep data/ beside the executable inside the bundle (Contents/MacOS/data),
    # matching the runtime lookup used on the other platforms.
    add_executable(LogicalSystem MACOSX_BUNDLE ${PROJECT_SRCS})
    set_target_properties(LogicalSystem PROPERTIES
            MACOSX_BUNDLE_BUNDLE_NAME "Logical System"
            MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.kewldan.logicalsystem"
            MACOSX_BUNDLE_ICON_FILE "LogicalSystem"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    target_sources(LogicalSystem PRIVATE resources/LogicalSystem.icns)
    set_source_files_properties(resources/LogicalSystem.icns PROPERTIES
            MACOSX_PACKAGE_LOCATION "Resources")
else ()
    add_executable(LogicalSystem ${PROJECT_SRCS})
endif ()

target_compile_definitions(LogicalSystem PRIVATE BLOCK_BATCHING=8192 LS_VERSION="${PROJECT_VERSION}")
target_include_directories(LogicalSystem PRIVATE include ${Stb_INCLUDE_DIR})
target_link_libraries(LogicalSystem PRIVATE Engine glad::glad glfw glm::glm plog::plog ZLIB::ZLIB aklomp::base64 imgui::imgui nfd::nfd nlohmann_json::nlohmann_json)

# Runtime asset lookup is relative to the process working directory. macOS
# launches an app bundle in Contents/Resources; other platforms use the
# executable directory.
if (APPLE)
    set(LS_DATA_DIR "$<TARGET_BUNDLE_CONTENT_DIR:LogicalSystem>/Resources/data")
else ()
    set(LS_DATA_DIR "$<TARGET_FILE_DIR:LogicalSystem>/data")
endif ()
add_custom_command(TARGET LogicalSystem POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/data ${LS_DATA_DIR})

enable_testing()
add_executable(LogicalSystemTests
        tests/tests.cpp
        src/Block.cpp
        src/AppStorage.cpp
        src/BlockCatalog.cpp
        src/CameraFrame.cpp
        src/Simulation.cpp
        src/Scheme.cpp
        src/SmoothZoom.cpp
)
target_include_directories(LogicalSystemTests PRIVATE include ${Stb_INCLUDE_DIR})
target_link_libraries(LogicalSystemTests PRIVATE doctest::doctest Engine plog::plog nlohmann_json::nlohmann_json ZLIB::ZLIB)
add_test(NAME LogicalSystemTests COMMAND LogicalSystemTests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
