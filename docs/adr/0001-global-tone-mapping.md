# ADR-0001 — Global tone mapping with a soft-knee highlight roll-off

- **Status:** Accepted
- **Date:** 2026-10-14
- **References:** [../spec/0001-relativistic-jet.md](../spec/0001-relativistic-jet.md)

## Context

The compute shader accumulated HDR radiance in `RGBA32F` and then clamped the
linear result straight to `[0, 1]` for display. Bright features — Doppler-boosted
disk light, photon-ring glow, the Einstein-ring point source — saturated to flat
white blocks instead of rolling off gracefully. The upcoming relativistic jet
would add more high-dynamic-range emission on top of this, making the clipping
worse.

## Decision

Apply a **global tone-mapping step at the end of the compute shader's `main()`**,
after all colour accumulation:

1. scale by a runtime `exposure`;
2. apply a **soft-knee highlight roll-off** — the mapping is the identity below
   `knee = 0.8` and only compresses true HDR headroom in `[knee, knee + width]`,
   asymptoting to `1.0`;
3. apply an optional display `gamma`.

`exposure` and `gamma` are runtime parameters (config + ImGui). Their defaults
(`exposure = 1.0`, `gamma = 1.0`) leave the normal range untouched, so the
existing disk/starfield look is preserved while highlights no longer clip.

## Alternatives considered

- **Hard clamp** (status quo) — rejected: clips to flat white.
- **Reinhard / ACES filmic** — rejected: both remap the whole range and would
  visibly change the existing image at default exposure, violating the
  "nearly unchanged" requirement. The soft-knee operator keeps the normal range
  bit-for-bit identical.

## Consequences

- Tone mapping is a prerequisite the jet depends on, so it shipped first
  (commit `c97d96e`).
- The display transform lives in one place at the end of `raytrace.comp`; the
  disk's own Doppler model is unchanged.
- Tone-mapped LDR output saturates above the knee, which is expected and must be
  accounted for when reading luminance ratios off rendered images.
