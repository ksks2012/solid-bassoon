# ADR-0002 — Ship the disk inner radius at the ISCO

- **Status:** Accepted
- **Date:** 2026-10-14
- **References:** [../spec/0001-relativistic-jet.md](../spec/0001-relativistic-jet.md)

## Context

The shipped default disk inner radius was `3.0` (`1.5·rs`) — the **photon
sphere**, not a physically defensible inner edge for a thin disk — and its
source comment mislabelled that value as the ISCO. A thin accretion disk is
truncated at the **ISCO** (`3·rs = 6M`), the innermost stable circular orbit.

## Decision

Change the disk `inner_radius` default **`3.0 → 6.0` (`3·rs`)** unconditionally,
and correct the misleading comment (the old value is the photon sphere). This is
a bug fix to the shipped default, not a new feature; the user can still set any
inner radius.

## Alternatives considered

- **Leave `3.0`** — rejected: physically wrong default; the UI already warned
  about it.
- **Make it a build-time/config-only change without a warning** — rejected: the
  existing UI warning style is kept so an out-of-range inner radius is still
  flagged.
- **Hard-clamp the shader to `≥ ISCO`** — rejected: too restrictive; users may
  intentionally explore non-ISCO configurations.

## Consequences

- With the jet off, the only change versus the pre-P2.1 render is the disk's
  inner edge — the basis of the ticket-0007 regression criterion.
- The disk inner radius and the jet `base_radius` are kept independent; the
  design rule `base_radius < disk_inner` (soft, warned) keeps the jet foot in
  the dark inner hole.
