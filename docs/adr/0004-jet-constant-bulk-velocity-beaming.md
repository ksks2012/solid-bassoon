# ADR-0004 — Model jet beaming with a single constant bulk velocity

- **Status:** Accepted
- **Date:** 2026-10-14
- **References:** [../spec/0001-relativistic-jet.md](../spec/0001-relativistic-jet.md)

## Context

A relativistic jet's appearance depends strongly on viewing angle: the lobe
moving toward the observer is brightened (Doppler beaming) and the receding lobe
dimmed. The renderer needed a beaming model that is physically plausible,
tunable with few parameters, and cheap enough to run per geodesic step.

## Decision

Use a **single constant bulk velocity** along the axis,
`v = sign(z)·β·ẑ`, and modulate the emission rate by the Doppler factor raised
to a spectral power:

```
n̂  = ( f·p_r, √f·p_θ/r, √f·p_φ/(r·sinθ) )      (observer → scene = march dir)
ẑ  = cosθ·ê_r − sinθ·ê_θ                        (jet axis, local frame)
δ  = 1 / [ γ·(1 − β·(v̂·(−n̂))) ],  γ = 1/√(1−β²)
gain = δ^(3+α)                                   (α = spectral_index)
```

The emitter → observer direction is the reverse of the backward-traced march, so
the sign uses `−n̂` (the classic trap, verified visually). `β = 0` returns an
unbeamed jet. Both `β` and `α` are runtime-tunable.

## Alternatives considered

- **Accelerating profile `β(z)`** — **deferred**: more parameters and a
  larger model; a constant β captures the qualitative asymmetry.
- **Full special-relativistic radiative transfer** — deferred with ADR-0003.
- **Beaming applied to the cumulative colour rather than per step** — rejected:
  it would not be step-independent and would mis-weight along the ray.

## Consequences

- Beaming composes **multiplicatively** with the emission profile — `j` is the
  only thing scaled, so there is no double counting.
- The direction is `sign(z)`-bipolar, so flipping `β` swaps which lobe is bright.
- Known approximation: the camera's momentum convention makes the spec's `n̂`
  evaluate to `f·`(unit direction) rather than a unit vector, slightly
  under-scaling beaming near the base (see CONTEXT §6). The implementation is
  faithful to the spec formula.
