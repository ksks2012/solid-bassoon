#include "app/window.h"
#include "logging/logger.h"
#include <stdexcept>
#include <string>

static void glfwErrorCallback(int error, const char* description) {
    LOG_ERROR("GLFW", "Error " + std::to_string(error) + ": " + description);
}

Window::Window(const std::string& title, int width, int height)
    : width_(width), height_(height)
{
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialise GLFW");
    }

    // Request OpenGL 4.3 core (minimum for Compute Shaders)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    window_ = glfwCreateWindow(width_, height_, title.c_str(), nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);  // vsync

    // Initialise GLEW after context creation
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        glfwDestroyWindow(window_);
        glfwTerminate();
        throw std::runtime_error(
            std::string("Failed to initialise GLEW: ") +
            reinterpret_cast<const char*>(glewGetErrorString(err)));
    }

    // Verify Compute Shader support
    if (!GLEW_ARB_compute_shader) {
        LOG_WARN("Window", "ARB_compute_shader not available — GPU ray tracing disabled");
    }

    LOG_INFO("Window", "OpenGL " +
             std::string(reinterpret_cast<const char*>(glGetString(GL_VERSION))));
    LOG_INFO("Window", "Renderer: " +
             std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))));

    // Query compute shader limits
    GLint maxWorkGroupCount[3], maxWorkGroupSize[3], maxInvocations;
    for (int i = 0; i < 3; ++i) {
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &maxWorkGroupCount[i]);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE,  i, &maxWorkGroupSize[i]);
    }
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &maxInvocations);

    LOG_INFO("Window", "Compute work group max count: " +
             std::to_string(maxWorkGroupCount[0]) + " x " +
             std::to_string(maxWorkGroupCount[1]) + " x " +
             std::to_string(maxWorkGroupCount[2]));
    LOG_INFO("Window", "Compute work group max size: " +
             std::to_string(maxWorkGroupSize[0]) + " x " +
             std::to_string(maxWorkGroupSize[1]) + " x " +
             std::to_string(maxWorkGroupSize[2]));
    LOG_INFO("Window", "Max compute invocations per work group: " +
             std::to_string(maxInvocations));
}

Window::~Window() {
    if (window_) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_);
}

void Window::swapBuffers() {
    glfwSwapBuffers(window_);
}

void Window::pollEvents() {
    glfwPollEvents();
}
