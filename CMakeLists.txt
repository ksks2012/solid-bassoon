cmake_minimum_required(VERSION 3.10)
project(BlackholeRenderer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_C_COMPILER_LAUNCHER ccache)
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ── Include paths ──────────────────────────────────────────────────────────────
include_directories(${CMAKE_SOURCE_DIR}/include)

# spdlog (header-only)
set(SPDLOG_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/lib/spdlog/)
file(GLOB SPDLOG_SOURCES ${SPDLOG_INCLUDE_DIRS}/*.h)

# imgui
set(IMGUI_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/lib/imgui)
file(GLOB IMGUI_SOURCES ${IMGUI_INCLUDE_DIRS}/*.cpp)

# nlohmann/json (header-only, lives under lib/)
set(NLOHMANN_JSON_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/lib)

include_directories(
    ${SPDLOG_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIRS}
    ${NLOHMANN_JSON_INCLUDE_DIRS}
)

# Tell imgui to use GLEW instead of its built-in loader
add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_CUSTOM)

# ── Source files ───────────────────────────────────────────────────────────────
file(GLOB CORE_SOURCES      src/core/*.cpp)
file(GLOB PHYSICS_SOURCES   src/physics/*.cpp)
file(GLOB RENDERING_SOURCES src/rendering/*.cpp)
file(GLOB LOGGING_SOURCES   src/logging/*.cpp)
file(GLOB APP_SOURCES       src/app/*.cpp)
set(MAIN_SOURCE src/main.cpp)

# Force-include GL/glew.h before compiling imgui sources so that
# GL types and functions are available when IMGUI_IMPL_OPENGL_LOADER_CUSTOM is set.
set_source_files_properties(${IMGUI_SOURCES}
    PROPERTIES COMPILE_FLAGS "-include GL/glew.h"
)

set(SOURCES
    ${MAIN_SOURCE}
    ${CORE_SOURCES}
    ${PHYSICS_SOURCES}
    ${RENDERING_SOURCES}
    ${LOGGING_SOURCES}
    ${APP_SOURCES}
    ${IMGUI_SOURCES}
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    ${OPENGL_LIBRARIES}
    glfw
    GLEW::GLEW
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
