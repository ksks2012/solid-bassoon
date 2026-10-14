# 0004 — First visible jet: cone + optically-thin accumulation

**Spec:** [../spec/0001-relativistic-jet.md](../spec/0001-relativistic-jet.md)

**What to build:** The first end-to-end, visible jet. A bipolar solid cone around the symmetry axis with a transverse Gaussian profile, launched outside the photon sphere and inside the disk's inner edge, sampled once per geodesic step, integrated over the **local proper length** `ds = h_eff / sqrt(f)`, and **added** onto every terminal colour (captured / disk / escaped / max-steps). It lenses correctly through the existing marcher and, being optically thin, does not occlude the disk, stars, or Einstein rings behind it.

**Blocked by:** 0001, 0003 — can start once both are done.

**Status:** blocked (awaiting 0001, 0003)

- [ ] Enabling the jet draws a visible bipolar jet along the axis.
- [ ] The jet does not occlude the disk, stars, or Einstein rings (optically thin, additive over all terminal colours).
- [ ] The jet light lenses correctly (its shape bends with the geodesics) and remains visible in front of the shadow.
- [ ] Jet appearance is step-independent: changing the base step size does not change its brightness.
- [ ] Per step, jet emission is accumulated **before** the disk-crossing test, so a ray that terminates on the disk carries exactly the foreground jet accumulated up to the hit (the disk behind the jet is not double-counted).
- [ ] The jet base sits within the disk's dark inner hole and does not intrude into the emitting disk.
