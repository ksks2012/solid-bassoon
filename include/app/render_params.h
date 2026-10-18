#ifndef RENDER_PARAMS_H
#define RENDER_PARAMS_H

#include "app/config.h"
#include "rendering/ray_tracer.h"

// Translate a loaded Config into the uniform block consumed by the GPU ray
// tracer. Shared by the interactive loop and the headless `--render` entry
// point so the two paths cannot drift.
RayTracerParams buildRayTracerParams(const Config& config);

#endif // RENDER_PARAMS_H
