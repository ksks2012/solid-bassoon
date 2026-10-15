#include "app/window.h"
#include "app/config.h"
#include "logging/logger.h"
#include "logging/spdlog_logger.h"
#include "core/metric.h"
#include "core/geodesic_integrator.h"
#include "rendering/ray_tracer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <cmath>
#include <string>

// ── CPU validation helper ─────────────────────────────────────────────────────
// Trace a few test rays on the CPU at startup to verify the integrator.

static void cpuValidation(const SchwarzschildMetric& metric, const Config& config) {
    // Use a larger step budget for validation — adaptive stepping near the
    // horizon shrinks the step size significantly, so we need many more
    // iterations to let the ray reach the horizon buffer.
    const int    validationMaxSteps = 20000;
    const double validationStepSize = 0.1;

    GeodesicIntegrator integrator(metric,
                                  validationStepSize,
                                  validationMaxSteps,
                                  config.render.abort_distance);

    const double r_obs     = config.camera.distance;
    const double theta_obs = config.camera.inclination * M_PI / 180.0;
    const double rs        = metric.schwarzschildRadius();
    const double f_obs     = 1.0 - rs / r_obs;
    const double sqrtF     = std::sqrt(f_obs);

    auto statusStr = [](RayStatus s) -> const char* {
        switch (s) {
            case RayStatus::CAPTURED:  return "CAPTURED";
            case RayStatus::ESCAPED:   return "ESCAPED";
            case RayStatus::MAX_STEPS: return "MAX_STEPS";
            default:                   return "ACTIVE";
        }
    };

    // Test rays using the local orthonormal frame model (same as GPU).
    // Each ray is defined by (alpha, beta) deflection angles from boresight.
    //   n_r  = -cos(beta)*cos(alpha)
    //   n_th =  sin(beta)
    //   n_ph =  cos(beta)*sin(alpha)
    //   p_t  = -1,  p_r = n_r,  p_th = r*sqrt(f)*n_th,  p_ph = r*sin(th)*sqrt(f)*n_ph
    struct TestRay { const char* name; double alpha_deg; double beta_deg; };
    TestRay tests[] = {
        { "radial ingoing",       0.0,  0.0 },   // straight toward BH → CAPTURED
        { "slight offset (2°)",   2.0,  0.0 },   // small deflection   → CAPTURED
        { "large offset (20°)",  20.0,  0.0 },   // should escape      → ESCAPED
    };

    for (auto& t : tests) {
        double alpha = t.alpha_deg * M_PI / 180.0;
        double beta  = t.beta_deg  * M_PI / 180.0;

        double n_r  = -std::cos(beta) * std::cos(alpha);
        double n_th =  std::sin(beta);
        double n_ph =  std::cos(beta) * std::sin(alpha);

        double pt  = -1.0;
        double pr  = n_r;
        double pth = r_obs * sqrtF * n_th;
        double pph = r_obs * std::sin(theta_obs) * sqrtF * n_ph;

        SchwarzschildMetric::State s0;
        s0.x = {0.0, r_obs, theta_obs, 0.0};
        s0.p = {pt, pr, pth, pph};

        TraceResult result = integrator.trace(s0);

        LOG_INFO("CPUTest", std::string(t.name) +
                 ": status=" + statusStr(result.status) +
                 "  steps=" + std::to_string(result.steps) +
                 "  r_final=" + std::to_string(result.finalState.x[1]));
    }
}

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

    // Detect software renderer
    const char* glRendererCStr = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::string glRendererStr = glRendererCStr ? glRendererCStr : "unknown";
    bool isSoftwareRenderer = (glRendererStr.find("llvmpipe") != std::string::npos ||
                               glRendererStr.find("swrast")   != std::string::npos);
    if (isSoftwareRenderer) {
        LOG_WARN("Main", "Software renderer detected (" + glRendererStr +
                 ") — expect very low FPS for compute shader dispatch");
    }

    // ── CPU geodesic validation ───────────────────────────────────────────────
    cpuValidation(metric, config);

    // ── GPU ray tracer ────────────────────────────────────────────────────────
    RayTracer rayTracer;
    if (!rayTracer.init(config.render.width, config.render.height)) {
        LOG_ERROR("Main", "Failed to initialise GPU ray tracer");
        return 1;
    }

    RayTracerParams rtParams;
    rtParams.mass            = static_cast<float>(config.blackhole.mass);
    rtParams.rs              = static_cast<float>(config.blackhole.rs);
    rtParams.cam_distance    = static_cast<float>(config.camera.distance);
    rtParams.cam_inclination = static_cast<float>(config.camera.inclination);
    rtParams.cam_fov         = static_cast<float>(config.camera.fov);
    rtParams.max_steps       = config.render.max_steps;
    rtParams.step_size       = static_cast<float>(config.render.step_size);
    rtParams.abort_distance  = static_cast<float>(config.render.abort_distance);
    rtParams.exposure        = static_cast<float>(config.render.exposure);
    rtParams.gamma           = static_cast<float>(config.render.gamma);

    // Accretion disk from config
    rtParams.disk_inner   = static_cast<float>(config.disk.inner_radius);
    rtParams.disk_outer   = static_cast<float>(config.disk.outer_radius);
    rtParams.disk_temp    = static_cast<float>(config.disk.temperature);
    rtParams.disk_enabled = 1;

    // ── Main loop ─────────────────────────────────────────────────────────────
    bool needsRender = true;  // only dispatch compute when params change

    while (!window.shouldClose()) {
        window.pollEvents();

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ── Info & control panel ──────────────────────────────────────────────
        ImGui::Begin("Schwarzschild Black Hole");

        // FPS / renderer info
        ImGui::Text("FPS: %.1f (%.1f ms)", ImGui::GetIO().Framerate,
                     1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("Renderer: %s", glRendererStr.c_str());
        if (isSoftwareRenderer) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                               "Software rendering — no GPU acceleration");
        }

        ImGui::Separator();
        ImGui::Text("Mass M = %.3f", metric.mass());
        ImGui::Text("Schwarzschild radius rs = %.3f", metric.schwarzschildRadius());
        ImGui::Text("Photon sphere r = %.3f", metric.photonSphere());
        ImGui::Text("ISCO r = %.3f", metric.iscoRadius());

        ImGui::Separator();
        ImGui::Text("Camera");
        bool changed = false;
        changed |= ImGui::SliderFloat("Distance",    &rtParams.cam_distance,    5.0f, 100.0f);
        changed |= ImGui::SliderFloat("Inclination", &rtParams.cam_inclination, 1.0f, 179.0f);
        changed |= ImGui::SliderFloat("FOV",         &rtParams.cam_fov,         10.0f, 120.0f);

        ImGui::Separator();
        ImGui::Text("Integration");
        changed |= ImGui::SliderFloat("Step size",      &rtParams.step_size,      0.01f, 1.0f, "%.3f");
        changed |= ImGui::SliderInt("Max steps",        &rtParams.max_steps,      100, 50000);
        changed |= ImGui::SliderFloat("Abort distance", &rtParams.abort_distance, 20.0f, 500.0f);

        // ── Einstein Ring: point light source ─────────────────────────────────
        ImGui::Separator();
        ImGui::Text("Point Source (Einstein Ring)");

        changed |= ImGui::SliderFloat("Source Theta",  &rtParams.src_theta,      1.0f, 179.0f, "%.1f deg");
        changed |= ImGui::SliderFloat("Source Phi",    &rtParams.src_phi,        0.0f, 360.0f, "%.1f deg");
        changed |= ImGui::SliderFloat("Source Radius", &rtParams.src_radius,     0.1f, 5.0f,   "%.2f deg");
        changed |= ImGui::SliderFloat("Brightness",    &rtParams.src_brightness, 0.0f, 10.0f,  "%.1f");

        // Preset: perfect Einstein ring (source directly behind BH)
        if (ImGui::Button("Einstein Ring Preset")) {
            rtParams.src_theta      = 180.0f - rtParams.cam_inclination;  // match camera θ
            rtParams.src_phi        = 180.0f;                    // directly behind BH
            rtParams.src_radius     = 0.5f;
            rtParams.src_brightness = 5.0f;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Source Off")) {
            rtParams.src_brightness = 0.0f;
            changed = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("When the source is at (theta=cam_incl, phi=180),");
            ImGui::Text("it lies directly behind the BH, producing a");
            ImGui::Text("complete Einstein ring via gravitational lensing.");
            ImGui::EndTooltip();
        }

        // ── Accretion Disk ────────────────────────────────────────────────────
        ImGui::Separator();
        ImGui::Text("Accretion Disk");

        bool diskOn = (rtParams.disk_enabled != 0);
        if (ImGui::Checkbox("Enable Disk", &diskOn)) {
            rtParams.disk_enabled = diskOn ? 1 : 0;
            changed = true;
        }

        if (diskOn) {
            changed |= ImGui::SliderFloat("Inner Radius", &rtParams.disk_inner, rtParams.rs * 1.05f, 20.0f, "%.1f");
            changed |= ImGui::SliderFloat("Outer Radius", &rtParams.disk_outer, rtParams.disk_inner + 1.0f, 50.0f, "%.1f");
            changed |= ImGui::SliderFloat("Temperature",  &rtParams.disk_temp,  0.1f, 3.0f, "%.2f");

            // Info text
            float isco = 3.0f * rtParams.rs;
            ImGui::Text("ISCO = %.1f  (3 rs)", isco);
            if (rtParams.disk_inner < isco) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                                   "Inner radius < ISCO (unphysical)");
            }
        }

        // ── Display / tone mapping ────────────────────────────────────────────
        ImGui::Separator();
        ImGui::Text("Display (Tone Mapping)");
        changed |= ImGui::SliderFloat("Exposure", &rtParams.exposure, 0.1f, 4.0f, "%.2f");
        changed |= ImGui::SliderFloat("Gamma",    &rtParams.gamma,    1.0f, 3.0f, "%.2f");

        ImGui::Separator();
        ImGui::Text("Render: %d x %d", config.render.width, config.render.height);

        if (ImGui::Button("Force Re-render")) changed = true;

        ImGui::End();

        needsRender |= changed;

        // ── Render ────────────────────────────────────────────────────────────
        glViewport(0, 0, window.getWidth(), window.getHeight());
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (needsRender) {
            rayTracer.render(rtParams);
            needsRender = false;
        } else {
            rayTracer.drawLastFrame();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        window.swapBuffers();
    }

    // ── Cleanup ───────────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
