#include "app/window.h"
#include "app/config.h"
#include "logging/logger.h"
#include "logging/spdlog_logger.h"
#include "core/metric.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <cmath>

int main(int argc, char* argv[]) {
    // ── Logger setup ──────────────────────────────────────────────────────────
    auto logger = std::make_shared<SpdlogLogger>();
    GlobalLogger::set(logger);

    // ── Config ────────────────────────────────────────────────────────────────
    Config config;
    try {
        config.loadFromFile("etc/config.json");
    } catch (const ConfigError& e) {
        LOG_ERROR("Main", std::string("Config error: ") + e.what());
        return 1;
    }

    // Apply log level from config
    LogLevel logLevel = static_cast<LogLevel>(config.logger_level);
    logger->set_level(logLevel);

    // ── Window + OpenGL context ───────────────────────────────────────────────
    Window window("Blackhole Renderer", config.render.width, config.render.height);

    // ── ImGui setup ───────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window.getHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 430");

    // ── Schwarzschild metric info ─────────────────────────────────────────────
    SchwarzschildMetric metric(config.blackhole.mass);
    LOG_INFO("Main", "Black hole mass M = " + std::to_string(metric.mass()));
    LOG_INFO("Main", "Schwarzschild radius rs = " + std::to_string(metric.schwarzschildRadius()));
    LOG_INFO("Main", "Photon sphere r = " + std::to_string(metric.photonSphere()));
    LOG_INFO("Main", "ISCO radius r = " + std::to_string(metric.iscoRadius()));

    // ── Main loop ─────────────────────────────────────────────────────────────
    while (!window.shouldClose()) {
        window.pollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Info panel
        ImGui::Begin("Schwarzschild Black Hole");
        ImGui::Text("Mass M = %.3f", metric.mass());
        ImGui::Text("Schwarzschild radius rs = %.3f", metric.schwarzschildRadius());
        ImGui::Text("Photon sphere r = %.3f", metric.photonSphere());
        ImGui::Text("ISCO r = %.3f", metric.iscoRadius());
        ImGui::Separator();
        ImGui::Text("Render: %d x %d", config.render.width, config.render.height);
        ImGui::Text("Max steps: %d", config.render.max_steps);
        ImGui::Text("Step size: %.4f", config.render.step_size);
        ImGui::End();

        // Render
        ImGui::Render();
        glViewport(0, 0, window.getWidth(), window.getHeight());
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        window.swapBuffers();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
