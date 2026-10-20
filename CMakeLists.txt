cmake_minimum_required(VERSION 3.10)
project(BlackholeRenderer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_C_COMPILER_LAUNCHER ccache)
set(CMAKE_CXX_COMPILER_LAUNCHER ccache)

# Output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# ── Third-party include roots (vendored under lib/, gitignored) ───────────────
set(SPDLOG_INCLUDE_DIRS    ${CMAKE_SOURCE_DIR}/lib/spdlog)
set(IMGUI_INCLUDE_DIRS     ${CMAKE_SOURCE_DIR}/lib/imgui)
set(NLOHMANN_INCLUDE_DIRS  ${CMAKE_SOURCE_DIR}/lib)

# ══════════════════════════════════════════════════════════════════════════════
# blackhole_core — pure, runtime-independent library.
#
# MUST stay free of GL / GLFW / ImGui includes and flags; the test targets link
# this library only (see docs/adr/0005-extract-blackhole-core.md).
# ══════════════════════════════════════════════════════════════════════════════
add_library(blackhole_core STATIC
    src/core/metric.cpp
    src/core/geodesic_integrator.cpp
    src/core/emission_reference.cpp
    src/app/config.cpp
    src/logging/spdlog_logger.cpp
)
target_include_directories(blackhole_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${SPDLOG_INCLUDE_DIRS}
    ${NLOHMANN_INCLUDE_DIRS}
)

# ══════════════════════════════════════════════════════════════════════════════
# BlackholeRenderer — the application (GL context, window, ImGui, renderer).
# ══════════════════════════════════════════════════════════════════════════════
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# imgui
file(GLOB IMGUI_SOURCES ${IMGUI_INCLUDE_DIRS}/*.cpp)

# Force-include GL/glew.h before compiling imgui sources so that GL types and
# functions are available when IMGUI_IMPL_OPENGL_LOADER_CUSTOM is set.
set_source_files_properties(${IMGUI_SOURCES}
    PROPERTIES COMPILE_FLAGS "-include GL/glew.h"
)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/app/window.cpp
    src/app/render_params.cpp
    src/app/headless.cpp
    src/rendering/ray_tracer.cpp
    ${IMGUI_SOURCES}
)
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_INCLUDE_DIRS})
# Tell imgui to use GLEW instead of its built-in loader
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_CUSTOM)
target_link_libraries(${PROJECT_NAME} PRIVATE
    blackhole_core
    ${OPENGL_LIBRARIES}
    glfw
    GLEW::GLEW
)

# ══════════════════════════════════════════════════════════════════════════════
# Tests
# ══════════════════════════════════════════════════════════════════════════════
enable_testing()
add_subdirectory(tests)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
