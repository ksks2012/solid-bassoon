# ADR-0003 — Emission-only, optically-thin relativistic jet

- **Status:** Accepted
- **Date:** 2026-10-14
- **References:** [../spec/0001-relativistic-jet.md](../spec/0001-relativistic-jet.md)

## Context

The renderer lacked a jet, the most recognisable feature of an accreting black
hole. Adding one raises a modelling question: does the jet interact with the
background light (absorption / optical depth), or is it purely additive? A full
relativistic radiative-transfer treatment with self-absorption is substantial
work.

## Decision

Ship the jet as an **emission-only, optically-thin foreground**: per geodesic
step it accumulates emission `j · ds` along the **local proper length**
`ds = h_eff / √f`, and **adds** the total onto whatever the ray terminates on
(disk, sky, captured, max-steps). The jet never absorbs or occludes background
light and needs no special-casing per terminal colour.

## Alternatives considered

- **Full radiative transport (Phase 5):** self-absorption, optical depth `τ`,
  and absorption of background light — **deferred** beyond P2.1; it is a
  different, much larger model and would couple the jet to every background.
- **Opaque / geometry-clipped jet** — rejected: would occlude the disk and
  Einstein ring and look like a solid tube.
- **Separate render pass** — rejected: the per-step accumulation reuses the
  geodesic march, is step-independent, and adds no new seam.

## Consequences

- The jet glows over every background without special cases (user stories
  12–14); a ray terminating on the disk carries exactly the foreground jet
  accumulated before the hit, so the disk is not double-counted.
- The disabled path is gated by `u_jet_enabled` and costs essentially nothing.
- Knowledge extracted: "optically thin" here means *emission-only*, not a
  scattering model.
