# 0003 — Jet parameter plumbing, toggle, and panel

**Spec:** [../spec/0001-relativistic-jet.md](../spec/0001-relativistic-jet.md)

**What to build:** An end-to-end parameter path for the jet — config block → runtime parameter block → GPU uniforms → shader-side enable gate → UI — with the jet **off by default**. This delivers a runtime-enableable but not-yet-drawn jet: a "Relativistic Jet" panel with grouped controls, a config block with defaults, a shader enable gate that costs essentially nothing when off, and a soft warning when the jet base is placed at or inside the disk inner radius. No jet geometry is drawn yet.

**Blocked by:** None — can start immediately (parallel to 0001).

**Status:** ready-for-agent

- [ ] A `relativistic_jet` config block is parsed, and missing keys fall back to struct defaults.
- [ ] The UI exposes the jet's parameters and an enable toggle, **off by default**.
- [ ] Enabling changes no visible pixels yet, but is observable (uniform/state), and the disabled path adds essentially no cost.
- [ ] A soft warning appears when the jet base radius is at or inside the disk inner radius.
- [ ] All uniform plumbing follows the existing per-uniform style, with no new UBO or test seam.
