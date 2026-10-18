#include "app/render_params.h"

RayTracerParams buildRayTracerParams(const Config& config) {
    RayTracerParams params;

    // Black hole + observer
    params.mass            = static_cast<float>(config.blackhole.mass);
    params.rs              = static_cast<float>(config.blackhole.rs);
    params.cam_distance    = static_cast<float>(config.camera.distance);
    params.cam_inclination = static_cast<float>(config.camera.inclination);
    params.cam_fov         = static_cast<float>(config.camera.fov);

    // Integration
    params.max_steps      = config.render.max_steps;
    params.step_size      = static_cast<float>(config.render.step_size);
    params.abort_distance = static_cast<float>(config.render.abort_distance);

    // Display / tone mapping
    params.exposure = static_cast<float>(config.render.exposure);
    params.gamma    = static_cast<float>(config.render.gamma);

    // Accretion disk
    params.disk_inner   = static_cast<float>(config.disk.inner_radius);
    params.disk_outer   = static_cast<float>(config.disk.outer_radius);
    params.disk_temp    = static_cast<float>(config.disk.temperature);
    params.disk_enabled = 1;

    // Relativistic jet (off by default)
    params.jet.enabled          = config.jet.enabled ? 1 : 0;
    params.jet.base_radius      = static_cast<float>(config.jet.base_radius);
    params.jet.opening_angle    = static_cast<float>(config.jet.opening_angle);
    params.jet.profile_exponent = static_cast<float>(config.jet.profile_exponent);
    params.jet.max_height       = static_cast<float>(config.jet.max_height);
    params.jet.radial_sigma     = static_cast<float>(config.jet.radial_sigma);
    params.jet.z_falloff        = static_cast<float>(config.jet.z_falloff);
    params.jet.axial_falloff    = static_cast<float>(config.jet.axial_falloff);
    params.jet.bulk_velocity    = static_cast<float>(config.jet.bulk_velocity);
    params.jet.spectral_index   = static_cast<float>(config.jet.spectral_index);
    params.jet.emissivity       = static_cast<float>(config.jet.emissivity);

    return params;
}
