#include "app/headless.h"

#include "app/config.h"
#include "core/bench_stats.h"
#include "app/render_params.h"
#include "app/window.h"
#include "core/probe_cases.h"
#include "logging/logger.h"
#include "rendering/ray_tracer.h"

#include <GL/glew.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <limits>
#include <memory>
#include <vector>

namespace {

// One sampling pass: `warmup` frames dispatched and discarded, then `frames`
// timed ones appended to `samples`. `label` names the pass in any failure.
//
// The gated loop and the stats pass go through this one function so their
// sampling protocol cannot drift apart — the report claims the two medians are
// comparable, and that claim rests on them sharing a warm-up and a frame count.
bool sampleFrames(RayTracer& rayTracer, const RayTracerParams& params,
                  bool statsEnabled, int warmup, int frames,
                  BenchCounters* counters, std::vector<double>& samples,
                  const char* label) {
    double gpuMs = 0.0;
    for (int i = 0; i < warmup; ++i) {
        if (!rayTracer.dispatchTimed(params, statsEnabled, gpuMs, counters)) {
            LOG_ERROR("Bench", std::string(label) + " dispatch failed during warm-up");
            return false;
        }
    }
    samples.reserve(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        if (!rayTracer.dispatchTimed(params, statsEnabled, gpuMs, counters)) {
            LOG_ERROR("Bench", std::string(label) + " dispatch failed");
            return false;
        }
        samples.push_back(gpuMs);
    }
    return true;
}

// Write the shape sidecar next to the raw buffer so a consumer can never
// silently interpret the bytes with the wrong dimensions.
bool writeSidecar(const std::string& path, int width, int height) {
    std::ofstream meta(path);
    if (!meta.is_open()) {
        return false;
    }
    meta << "width=" << width << "\n"
         << "height=" << height << "\n"
         << "channels=4\n"
         << "dtype=float32\n"
         << "layout=RGBA\n"
         << "byte_order=little\n"
         << "origin=bottom-left\n";
    return meta.good();
}

bool writeRawFloats(const std::string& path, const std::vector<float>& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size() * sizeof(float)));
    return out.good();
}

bool hasComputeContext() {
    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    bool versionOk = (major > 4) || (major == 4 && minor >= 3);
    return versionOk && GLEW_ARB_compute_shader;
}

}  // namespace

int runHeadless(const std::string& configPath, const std::string& outPath) {
    // ── Config ────────────────────────────────────────────────────────────────
    Config config;
    try {
        config.loadFromFile(configPath);
    } catch (const ConfigError& e) {
        LOG_ERROR("Headless", std::string("Config error: ") + e.what());
        return headless::kFail;
    }
    const int width  = config.render.width;
    const int height = config.render.height;

    // ── Hidden window + GL context ────────────────────────────────────────────
    std::unique_ptr<Window> window;
    try {
        window = std::make_unique<Window>("Blackhole Renderer (headless)",
                                          width, height, /*visible=*/false);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "SKIPPED: no OpenGL context available (%s)\n", e.what());
        return headless::kSkip;
    }

    if (!hasComputeContext()) {
        std::fprintf(stderr,
                     "SKIPPED: OpenGL 4.3 compute shaders unavailable on '%s'\n",
                     reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        return headless::kSkip;
    }

    // ── Render one frame ──────────────────────────────────────────────────────
    RayTracer rayTracer;
    if (!rayTracer.init(width, height)) {
        LOG_ERROR("Headless", "Failed to initialise GPU ray tracer");
        return headless::kFail;
    }

    rayTracer.render(buildRayTracerParams(config));
    glFinish();  // all compute writes retired before we read the texture

    std::vector<float> pixels;
    if (!rayTracer.readback(pixels)) {
        LOG_ERROR("Headless", "Output texture readback failed");
        return headless::kFail;
    }

    // ── Write the observation ─────────────────────────────────────────────────
    if (!writeRawFloats(outPath, pixels) || !writeSidecar(outPath + ".meta", width, height)) {
        LOG_ERROR("Headless", "Failed to write output: " + outPath);
        return headless::kFail;
    }

    LOG_INFO("Headless", "Rendered " + std::to_string(width) + "x" +
             std::to_string(height) + " scene to " + outPath);
    std::printf("RENDERED %dx%d -> %s\n", width, height, outPath.c_str());
    return headless::kOk;
}

int runBench(const std::string& configPath, int warmup, int frames) {
    if (warmup < 0 || frames <= 0) {
        LOG_ERROR("Bench", "frames must be positive and warmup non-negative");
        return headless::kFail;
    }

    // ── Config ─────────────────────────────────────────────────────────
    Config config;
    try {
        config.loadFromFile(configPath);
    } catch (const ConfigError& e) {
        LOG_ERROR("Bench", std::string("Config error: ") + e.what());
        return headless::kFail;
    }
    const int width  = config.render.width;
    const int height = config.render.height;

    // ── Hidden window + GL context ─────────────────────────────────────────────────────
    std::unique_ptr<Window> window;
    try {
        window = std::make_unique<Window>("Blackhole Renderer (bench)",
                                          width, height, /*visible=*/false);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "SKIPPED: no OpenGL context available (%s)\n", e.what());
        return headless::kSkip;
    }

    if (!hasComputeContext()) {
        std::fprintf(stderr,
                     "SKIPPED: OpenGL 4.3 compute shaders unavailable on '%s'\n",
                     reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        return headless::kSkip;
    }

    // ── Timed dispatches ──────────────────────────────────────────────────
    RayTracer rayTracer;
    if (!rayTracer.init(width, height)) {
        LOG_ERROR("Bench", "Failed to initialise GPU ray tracer");
        return headless::kFail;
    }

    const RayTracerParams params = buildRayTracerParams(config);

    // The stats SSBO is a 32-bit total, so it wraps above 4.29e9 steps.  Every
    // matrix config stays far below that in practice (rays terminate early),
    // but the theoretical worst case (pixels × max_steps) is worth saying out
    // loud rather than letting a wrapped diagnostic look plausible.
    const double worstCaseSteps =
        static_cast<double>(width) * static_cast<double>(height) *
        static_cast<double>(params.max_steps);
    if (worstCaseSteps >
        static_cast<double>(std::numeric_limits<std::uint32_t>::max())) {
        LOG_WARN("Bench", "worst-case step count " + std::to_string(
                     static_cast<long long>(worstCaseSteps)) +
                     " can wrap the 32-bit stats counter; steps_per_ray is "
                     "only meaningful while pixels · max_steps < 4.29e9");
    }

    std::vector<double> samples;
    if (!sampleFrames(rayTracer, params, /*statsEnabled=*/false, warmup,
                      frames, /*counters=*/nullptr, samples, "Timed")) {
        return headless::kFail;
    }

    // ── Diagnostic pass: same work, counters switched on ───────────────────────────
    // Same warm-up and frame count as the gated loop (the same helper runs
    // both, so that is structural rather than remembered); the difference
    // between the two medians is the cost of the per-step SSBO atomic,
    // reported and never gated.
    BenchCounters counters;
    std::vector<double> statsSamples;
    if (!sampleFrames(rayTracer, params, /*statsEnabled=*/true, warmup,
                      frames, &counters, statsSamples, "Stats")) {
        return headless::kFail;
    }

    // ── Publish ────────────────────────────────────────────────────────────────
    // The header and the row go out together as the last thing printed: the
    // gate locates the header line in this output and reads the very next one.
    bench::Frame frame;
    frame.summary = bench::summarize(samples);

    const GLubyte* renderer = glGetString(GL_RENDERER);
    frame.renderer = renderer
                         ? reinterpret_cast<const char*>(renderer)
                         : std::string("unknown");

    frame.steps_per_ray =
        counters.rays > 0
            ? static_cast<double>(counters.steps) /
                  static_cast<double>(counters.rays)
            : 0.0;

    const bench::Summary statsSummary = bench::summarize(statsSamples);
    frame.stats_median_ms = statsSummary.median_ms;
    frame.stats_overhead_pct =
        (frame.summary.median_ms > 0.0 && statsSummary.median_ms > 0.0)
            ? (statsSummary.median_ms - frame.summary.median_ms) /
                  frame.summary.median_ms * 100.0
            : 0.0;

    frame.width     = width;
    frame.height    = height;
    frame.max_steps = params.max_steps;
    frame.jet_enabled  = params.jet.enabled != 0;
    frame.disk_enabled = params.disk_enabled != 0;
    frame.frames    = frames;
    frame.warmup    = warmup;

    std::printf("%s\n%s\n", bench::csvHeader().c_str(),
                bench::csvRow(frame).c_str());
    std::fflush(stdout);

    LOG_INFO("Bench", "median " + std::to_string(frame.summary.median_ms) +
                      " ms over " + std::to_string(frames) +
                      " frames, " + std::to_string(frame.steps_per_ray) +
                      " RK4 steps/ray");
    return headless::kOk;
}

int runProbe(const std::string& outPath) {
    // ── Hidden window + GL context ────────────────────────────────────────────
    // Nothing is rendered: the probe only needs somewhere to own a compute
    // context, so the window stays tiny and invisible.
    std::unique_ptr<Window> window;
    try {
        window = std::make_unique<Window>("Blackhole Renderer (probe)",
                                          64, 64, /*visible=*/false);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "SKIPPED: no OpenGL context available (%s)\n", e.what());
        return headless::kSkip;
    }

    if (!hasComputeContext()) {
        std::fprintf(stderr,
                     "SKIPPED: OpenGL 4.3 compute shaders unavailable on '%s'\n",
                     reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
        return headless::kSkip;
    }

    // ── Dispatch the shared probe table ───────────────────────────────────────
    RayTracer rayTracer;
    std::vector<float> results;
    if (!rayTracer.runProbe(probe::packInputs(),
                            static_cast<int>(probe::kCases.size()), results)) {
        LOG_ERROR("Probe", "Probe dispatch failed");
        return headless::kFail;
    }

    // ── Write the observation ─────────────────────────────────────────────────
    // Same writer as the CPU reference (probe::writeProbeFile), so the two
    // files cannot drift apart structurally.
    if (!probe::writeProbeFile(outPath, results,
                               static_cast<int>(probe::kCases.size()), "gpu")) {
        LOG_ERROR("Probe", "Failed to write output: " + outPath);
        return headless::kFail;
    }

    LOG_INFO("Probe", "Probed " + std::to_string(probe::kCases.size()) +
                          " cases to " + outPath);
    std::printf("PROBED %zu cases -> %s\n", probe::kCases.size(), outPath.c_str());
    return headless::kOk;
}
