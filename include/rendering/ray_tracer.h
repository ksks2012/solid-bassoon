#ifndef RAY_TRACER_H
#define RAY_TRACER_H

#include <GL/glew.h>
#include <string>

// ═══════════════════════════════════════════════════════════════════════════════
// GPU Ray Tracer
//
// Manages the two-pass rendering pipeline:
//   Pass 1 — Compute Shader (raytrace.comp):
//            Each thread traces one pixel's null geodesic through the
//            Schwarzschild metric.  Writes colour into an RGBA32F image.
//
//   Pass 2 — Fullscreen Quad (fullscreen.vert + render.frag):
//            Samples the compute-shader output texture and presents it.
//
// Termination conditions (matching CPU integrator):
//   CAPTURED  (r ≤ rs)   → black
//   ESCAPED   (r > rmax) → background / sky colour
//   MAX_STEPS            → debug colour (indicates insufficient budget)
// ═══════════════════════════════════════════════════════════════════════════════

// Relativistic jet (optically thin, additive) — grouped runtime parameter block.
// Bipolar, axisymmetric, emission-only jet with Doppler beaming; off by default.
struct JetParams {
    int   enabled          = 0;      // 0 = off (default), 1 = on
    float base_radius      = 3.2f;   // cone foot radius at z = 0 (> photon sphere)
    float opening_angle    = 7.0f;   // half-opening angle (degrees)
    float profile_exponent = 1.0f;   // 1 = cone, 0.5 = parabola
    float max_height       = 40.0f;  // axial extent of the jet
    float radial_sigma     = 0.4f;   // transverse Gaussian width
    float z_falloff        = 10.0f;  // axial falloff length scale
    float axial_falloff    = 1.5f;   // axial power-law exponent
    float bulk_velocity    = 0.95f;  // β (constant)
    float spectral_index   = 0.7f;   // α in the beaming exponent δ^(3+α)
    float emissivity       = 1.0f;   // master brightness multiplier
};

struct RayTracerParams {
    // Black hole
    float mass          = 1.0f;
    float rs            = 2.0f;

    // Observer
    float cam_distance  = 30.0f;
    float cam_inclination = 80.0f;  // degrees
    float cam_fov       = 60.0f;    // degrees

    // Integration
    int   max_steps     = 1000;
    float step_size     = 0.05f;
    float abort_distance = 100.0f;

    // Point light source on the celestial sphere (Einstein ring)
    float src_theta      = 80.0f;   // colatitude (degrees) — default matches camera inclination
    float src_phi        = 180.0f;  // azimuth (degrees) — 180° = directly behind BH
    float src_radius     = 0.5f;    // angular radius (degrees)
    float src_brightness = 0.0f;    // brightness multiplier (0 = disabled)

    // Thin accretion disk (equatorial plane)
    float disk_inner     = 6.0f;    // inner radius (ISCO = 3·rs for Schwarzschild)
    float disk_outer     = 20.0f;   // outer radius
    float disk_temp      = 1.0f;    // peak temperature (normalised brightness)
    int   disk_enabled   = 1;       // 0 = off, 1 = on

    // Display / tone mapping
    float exposure       = 1.0f;    // linear exposure multiplier
    float gamma          = 1.0f;    // display gamma (1.0 = linear/identity)

    // Relativistic jet (grouped block)
    JetParams jet;
};

class RayTracer {
public:
    RayTracer();
    ~RayTracer();

    // Non-copyable
    RayTracer(const RayTracer&) = delete;
    RayTracer& operator=(const RayTracer&) = delete;

    // Initialise shaders and resources for the given resolution.
    // Call once after the OpenGL context is ready.
    bool init(int width, int height);

    // Dispatch the compute shader, then draw the fullscreen quad.
    void render(const RayTracerParams& params);

    // Re-draw the last computed result without re-dispatching compute.
    // Use this when parameters have not changed to avoid expensive re-trace.
    void drawLastFrame();

    // Resize the output texture (e.g. on window resize).
    void resize(int width, int height);

private:
    int width_  = 0;
    int height_ = 0;

    // Compute shader
    GLuint computeProgram_ = 0;
    GLuint outputTexture_  = 0;

    // Fullscreen quad
    GLuint quadProgram_ = 0;
    GLuint quadVAO_     = 0;
    GLuint quadVBO_     = 0;

    // Helpers
    GLuint compileShader(GLenum type, const std::string& source);
    GLuint linkProgram(GLuint shader);
    GLuint linkProgram(GLuint vertShader, GLuint fragShader);
    std::string loadShaderSource(const std::string& path);

    void createOutputTexture();
    void createFullscreenQuad();
};

#endif // RAY_TRACER_H
