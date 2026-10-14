# 0001 — Foundation: global tone mapping + disk ISCO fix

**Spec:** [../spec/0001-relativistic-jet.md](../spec/0001-relativistic-jet.md)

**What to build:** Two default-behaviour cleanups that every later slice depends on. (1) Every rendered frame passes through a global tone-mapping step (exposure + gamma 2.2) at the end of the compute path, so HDR highlights roll off gracefully; exposure and gamma are live-tunable and their defaults leave the current disk/starfield look essentially unchanged (suggested exposure start ≈ 1.0, tunable roughly 0.1–4.0, i.e. about −1.5 to +2 EV; gamma fixed at 2.2). (2) The shipped disk default inner radius moves to the ISCO (`3·rs`), and the incorrect "ISCO" comment on the old value is corrected (the old value was the photon sphere).

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] With the jet off, the default frame is nearly unchanged versus before, except the disk now starts at the ISCO.
- [ ] Bright HDR pixels (photon-ring glow, point source) roll off smoothly instead of clipping to flat white.
- [ ] Exposure and gamma are tunable at runtime (config + panel) with no code change, and the defaults preserve the current look.
- [ ] The disk default inner radius equals the ISCO and the misleading comment is gone; the UI shows no warning for defaults.
- [ ] The change is applied consistently wherever the display mapping lives, with no new test seam introduced.
