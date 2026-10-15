# 0001 — Foundation: global tone mapping + disk ISCO fix

**Spec:** [../spec/0001-relativistic-jet.md](../spec/0001-relativistic-jet.md)

**What to build:** Two default-behaviour cleanups that every later slice depends on. (1) Every rendered frame passes through a global tone-mapping step (exposure + a soft-knee highlight roll-off, then optional gamma) at the end of the compute path, so HDR highlights roll off gracefully; exposure and gamma are live-tunable and their defaults (exposure = 1.0, gamma = 1.0) leave the current disk/starfield look essentially unchanged. The soft knee is identity below `knee = 0.8` and only compresses true HDR headroom (approaching `knee + width`); exposure is tunable roughly 0.1–4.0 (about −1.5 to +2 EV) and gamma tunable up to 2.2. (2) The shipped disk default inner radius moves to the ISCO (`3·rs`), and the incorrect "ISCO" comment on the old value is corrected (the old value was the photon sphere).

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] With the jet off, the default frame is nearly unchanged versus before, except the disk now starts at the ISCO.
- [ ] Bright HDR pixels (photon-ring glow, point source) roll off smoothly instead of clipping to flat white.
- [ ] Exposure and gamma are tunable at runtime (config + panel) with no code change, and the defaults preserve the current look.
- [ ] The disk default inner radius equals the ISCO and the misleading comment is gone; the UI shows no warning for defaults.
- [ ] The change is applied consistently wherever the display mapping lives, with no new test seam introduced.
