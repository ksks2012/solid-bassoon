# Blackhole Renderer — Domain Glossary

A real-time renderer of a non-rotating (Schwarzschild) black hole: a static
observer's camera traces light rays backwards through curved spacetime to
produce gravitational lensing, a photon-sphere shadow, Einstein rings and a
thin accretion disk. This document is the single domain glossary; architecture
and decisions live in [CONTEXT.md](./CONTEXT.md) and [adr](./adr/).

## Spacetime & Geometry

**Schwarzschild metric**:
The spacetime geometry of a non-rotating, uncharged black hole, written in
Boyer–Lindquist coordinates. The only geometry this renderer implements.
_Avoid_: Kerr metric (rotating — not implemented), Einstein metric

**Boyer–Lindquist coordinates**:
The chart `(t, r, θ, φ)` in which the metric is diagonal: `t` time, `r`
areal-like radius, `θ` colatitude, `φ` azimuth.
_Avoid_: Cartesian coordinates, Kerr–Schild coordinates

**geometric units**:
The unit system `G = c = 1`, in which mass, length and time share one unit
(`M = 1` implies `rs = 2`).
_Avoid_: SI units

**Schwarzschild radius (rs)**:
`rs = 2M`, the radius of the event horizon.
_Avoid_: event horizon radius (the horizon *is* at `r = rs`, but the two names
are not interchangeable elsewhere)

**event horizon**:
The surface `r = rs`. Once inside, no outgoing path returns to the observer.
_Avoid_: singularity (that is `r = 0`), photon sphere

**photon sphere**:
The unstable circular photon orbit at `r = 1.5·rs = 3M`. Rays grazing it wind
many times around the hole and produce the photon ring.
_Avoid_: last photon orbit, ISCO

**ISCO** (innermost stable circular orbit):
The smallest radius at which a massive test particle has a stable circular
orbit: `r = 3·rs = 6M`. The physical inner edge of a thin disk.
_Avoid_: photon sphere, marginally stable orbit

## Ray Tracing

**ray**:
A single light path traced for one screen pixel, running backwards from the
camera. Represented as a point in the 8-dimensional phase space
`(t, r, θ, φ, p_t, p_r, p_θ, p_φ)`.
_Avoid_: photon (the physical particle; a ray is the numerical path), line

**null geodesic**:
A light path satisfying `ds² = 0`. Every ray in this renderer is a null
geodesic.
_Avoid_: light curve, trajectory

**affine parameter (λ)**:
The parameter along a null geodesic; integration advances the state in `λ`.
_Avoid_: time, arc length (neither is `λ` in general)

**covariant momentum (p_μ)**:
The momentum covector conjugate to `(t, r, θ, φ)`. `p_t = -E` and `p_φ = L` are
conserved because `∂_t` and `∂_φ` are Killing vectors.
_Avoid_: velocity, direction

**Hamiltonian formulation**:
The split-form geodesic equations used here: `dx^μ/dλ = g^{μν} p_ν` and
`dp_μ/dλ = -½ ∂g^{αβ}/∂x^μ · p_α p_β`.
_Avoid_: geodesic equation (ambiguous — name the formulation)

**backward ray tracing**:
Tracing from the camera into the scene rather than from light sources. All
rendering here is backward.
_Avoid_: forward tracing

**RK4** (fourth-order Runge–Kutta):
The fixed-form integrator used for each step of λ.
_Avoid_: Euler method

**adaptive step**:
The per-step scale factor that shrinks `Δλ` as `r → rs`, where the
Boyer–Lindquist chart is singular.
_Avoid_: variable timestep

**termination**:
How a ray's trace ends: **captured** (`r ≤ rs`, drawn black), **escaped**
(`r > abort_distance`, drawn from the sky), or **MAX_STEPS** (step budget
exhausted).
_Avoid_: escape/capture status

## Observer & Camera

**static observer**:
The camera: a non-rotating observer held at fixed `(r_obs, θ_obs)`, whose local
frame anchors all ray directions.
_Avoid_: camera position, viewpoint

**orthonormal tetrad**:
The observer's local frame `{ê_(t), ê_(r), ê_(θ), ê_(φ)}`, used to convert a
screen direction into covariant momenta with unit local energy.
_Avoid_: basis, frame (qualify as *local orthonormal*)

**inclination**:
The observer's colatitude `θ_obs`, measured from the spin axis: `0°` looks down
the axis, `90°` looks edge-on.
_Avoid_: elevation, latitude, pitch

**boresight**:
The screen-centre direction, pointing radially inward along `-ê_(r)`.
_Avoid_: optical axis, forward

## Visual Phenomena

**gravitational lensing**:
The apparent bending and displacement of background light caused by the curved
geometry.
_Avoid_: refraction, distortion

**photon-sphere shadow**:
The dark central region where rays are captured. Its edge sits at the critical
impact parameter `b = 3√3·M` (larger than the horizon).
_Avoid_: event-horizon image

**photon ring**:
The bright, thin ring just outside the shadow, formed by rays that wound near
the photon sphere. Brightness grows with the number of half-orbits.
_Avoid_: Einstein ring, glow

**Einstein ring**:
The complete ring image of a point source lying directly behind the black hole.
A key accuracy check on the integrator.
_Avoid_: photon ring, halo

**point source**:
An optional small bright emitter on the celestial sphere, parameterised by
colatitude, azimuth, angular radius and brightness. Used to produce the
Einstein ring.
_Avoid_: star (stars belong to the procedural starfield), light

**celestial sphere**:
The map from an escaped ray's direction `(θ, φ)` to a background colour — the
"sky" seen at infinity.
_Avoid_: skybox, background image

**starfield**:
The procedural, multi-layer pattern of stars drawn on the celestial sphere,
including a diffuse **Milky Way band** near the equator.
_Avoid_: textures, star map, galaxy, nebula

## Accretion Disk

**accretion disk**:
A thin emitting disk in the equatorial plane (`θ = π/2`), extending from an
inner to an outer radius.
_Avoid_: gas cloud, torus, halo

**thin disk**:
The geometrically thin, optically thick approximation used here. The disk has
no vertical extent and is opaque, so the first crossing ends the ray.
_Avoid_: volumetric disk, optically thin disk

**disk crossing**:
An event where a ray's colatitude passes through `π/2`; the crossing radius is
found by linear interpolation between steps.
_Avoid_: plane intersection

**temperature profile**:
The simplified Novikov–Thorne law
`T(r) ∝ r^(-3/4) · [1 - √(r_in/r)]^(1/4)` driving the disk's colour and
brightness.
_Avoid_: luminosity profile

**gravitational redshift**:
Dimming of disk light by `√f(r)` with `f = 1 - rs/r` at the emission point.
_Avoid_: redshift (qualify as *gravitational*)

**Doppler beaming**:
Brightening of light from gas moving toward the observer, approximated here as
the cube of the Doppler factor `δ³` from the Keplerian orbital velocity.
_Avoid_: aberration, blueshift

**blackbody palette**:
The fixed temperature-to-colour mapping: blue-white near the inner edge through
orange-red at the outer edge.
_Avoid_: colour map, gradient

## Rendering Pipeline

**RayTracerParams**:
The single runtime parameter block (black hole, camera, integration, point
source and disk settings) passed from the app to the GPU.
_Avoid_: uniforms (the GLSL names, not the domain concept)

**two-pass pipeline**:
A **compute shader** (`raytrace.comp`) traces one ray per pixel into an image;
a **fullscreen quad** then samples that image onto the window.
_Avoid_: kernel, blit, present pass
