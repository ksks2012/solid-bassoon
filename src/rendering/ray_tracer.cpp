#include "rendering/ray_tracer.h"
#include "logging/logger.h"

#include <fstream>
#include <sstream>
#include <vector>

// ── Shader helpers ────────────────────────────────────────────────────────────

std::string RayTracer::loadShaderSource(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("RayTracer", "Cannot open shader file: " + path);
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint RayTracer::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        LOG_ERROR("RayTracer", "Shader compile error:\n" + std::string(log.data()));
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint RayTracer::linkProgram(GLuint shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetProgramInfoLog(program, len, nullptr, log.data());
        LOG_ERROR("RayTracer", "Program link error:\n" + std::string(log.data()));
        glDeleteProgram(program);
        return 0;
    }

    glDetachShader(program, shader);
    glDeleteShader(shader);
    return program;
}

GLuint RayTracer::linkProgram(GLuint vertShader, GLuint fragShader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetProgramInfoLog(program, len, nullptr, log.data());
        LOG_ERROR("RayTracer", "Program link error:\n" + std::string(log.data()));
        glDeleteProgram(program);
        return 0;
    }

    glDetachShader(program, vertShader);
    glDetachShader(program, fragShader);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return program;
}

// ── Texture ───────────────────────────────────────────────────────────────────

void RayTracer::createOutputTexture() {
    if (outputTexture_) {
        glDeleteTextures(1, &outputTexture_);
    }
    glGenTextures(1, &outputTexture_);
    glBindTexture(GL_TEXTURE_2D, outputTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F,
                 width_, height_, 0, GL_RGBA, GL_FLOAT, nullptr);
    // Use NEAREST filtering — we want exact per-pixel colours from the
    // compute shader, no interpolation between adjacent ray results.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ── Fullscreen quad ───────────────────────────────────────────────────────────

void RayTracer::createFullscreenQuad() {
    // Two triangles covering the screen: (pos.xy, uv.xy)
    static const float quadVertices[] = {
        // pos       uv
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,

         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };

    glGenVertexArrays(1, &quadVAO_);
    glGenBuffers(1, &quadVBO_);

    glBindVertexArray(quadVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // attribute 0: position (vec2)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // attribute 1: uv (vec2)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

// ── Public API ────────────────────────────────────────────────────────────────

RayTracer::RayTracer() = default;

RayTracer::~RayTracer() {
    if (computeProgram_) glDeleteProgram(computeProgram_);
    if (quadProgram_)    glDeleteProgram(quadProgram_);
    if (outputTexture_)  glDeleteTextures(1, &outputTexture_);
    if (quadVBO_)        glDeleteBuffers(1, &quadVBO_);
    if (quadVAO_)        glDeleteVertexArrays(1, &quadVAO_);
}

bool RayTracer::init(int width, int height) {
    width_  = width;
    height_ = height;

    // ── Compute shader ────────────────────────────────────────────────────────
    std::string compSrc = loadShaderSource("shaders/raytrace.comp");
    if (compSrc.empty()) return false;

    GLuint compShader = compileShader(GL_COMPUTE_SHADER, compSrc);
    if (!compShader) return false;

    computeProgram_ = linkProgram(compShader);
    if (!computeProgram_) return false;

    LOG_INFO("RayTracer", "Compute shader compiled and linked");

    // ── Fullscreen quad shaders ───────────────────────────────────────────────
    std::string vertSrc = loadShaderSource("shaders/fullscreen.vert");
    std::string fragSrc = loadShaderSource("shaders/render.frag");
    if (vertSrc.empty() || fragSrc.empty()) return false;

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!vertShader || !fragShader) return false;

    quadProgram_ = linkProgram(vertShader, fragShader);
    if (!quadProgram_) return false;

    LOG_INFO("RayTracer", "Fullscreen quad shaders compiled and linked");

    // ── Resources ─────────────────────────────────────────────────────────────
    createOutputTexture();
    createFullscreenQuad();

    LOG_INFO("RayTracer", "Initialised (" + std::to_string(width_) + "x" +
             std::to_string(height_) + ")");
    return true;
}

void RayTracer::resize(int width, int height) {
    if (width == width_ && height == height_) return;
    width_  = width;
    height_ = height;
    createOutputTexture();
    LOG_INFO("RayTracer", "Resized to " + std::to_string(width_) + "x" +
             std::to_string(height_));
}

void RayTracer::render(const RayTracerParams& params) {
    // ── Pass 1: Compute shader — trace geodesics ──────────────────────────────
    glUseProgram(computeProgram_);

    // Bind output image
    glBindImageTexture(0, outputTexture_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    // Set uniforms
    glUniform1f(glGetUniformLocation(computeProgram_, "u_mass"),           params.mass);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_rs"),             params.rs);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_cam_distance"),   params.cam_distance);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_cam_inclination"),params.cam_inclination);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_cam_fov"),        params.cam_fov);
    glUniform1i(glGetUniformLocation(computeProgram_, "u_max_steps"),      params.max_steps);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_step_size"),      params.step_size);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_abort_distance"), params.abort_distance);
    glUniform2i(glGetUniformLocation(computeProgram_, "u_resolution"),     width_, height_);

    // Point light source (Einstein ring)
    glUniform1f(glGetUniformLocation(computeProgram_, "u_src_theta"),      params.src_theta);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_src_phi"),        params.src_phi);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_src_radius"),     params.src_radius);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_src_brightness"), params.src_brightness);

    // Thin accretion disk
    glUniform1f(glGetUniformLocation(computeProgram_, "u_disk_inner"),   params.disk_inner);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_disk_outer"),   params.disk_outer);
    glUniform1f(glGetUniformLocation(computeProgram_, "u_disk_temp"),    params.disk_temp);
    glUniform1i(glGetUniformLocation(computeProgram_, "u_disk_enabled"), params.disk_enabled);

    // Dispatch: one thread per pixel, work groups of 16×16
    GLuint groupsX = (width_  + 15) / 16;
    GLuint groupsY = (height_ + 15) / 16;
    glDispatchCompute(groupsX, groupsY, 1);

    // Ensure compute writes are visible to the fragment shader
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // ── Pass 2: Fullscreen quad — present the result ──────────────────────────
    // Disable blending/depth — ImGui may leave these enabled
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(quadProgram_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, outputTexture_);
    glUniform1i(glGetUniformLocation(quadProgram_, "u_texture"), 0);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void RayTracer::drawLastFrame() {
    // Re-draw the existing texture without re-dispatching compute shader.
    // Cheap — only the fullscreen quad pass.
    if (!quadProgram_ || !outputTexture_) return;

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(quadProgram_);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, outputTexture_);
    glUniform1i(glGetUniformLocation(quadProgram_, "u_texture"), 0);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}
