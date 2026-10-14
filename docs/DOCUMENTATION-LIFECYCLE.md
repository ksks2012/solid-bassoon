# Documentation Lifecycle Management

This document describes a documentation lifecycle management mechanism, following the **concentrate** principles. It is intentionally project-agnostic.

---

## 1. Principles Overview

### Three document kinds

Distinguish documents by the question they answer and by their lifecycle. Keeping them separate prevents a single file from mixing immutable decisions with living intent and transient work.

| Kind | Answers | Lifecycle | Unit of content |
|---|---|---|---|
| **ADR** | "why was this chosen, and what was rejected?" | immutable; superseded, never rewritten | one decision |
| **Spec** (PRD) | "what are we building, and how do we know it is done?" | evolves with the feature | one feature / intent |
| **Ticket** | "what work remains?" | archived when complete | one unit of work |

Dependencies point in one direction only, never in a cycle:

```
ticket ─→ spec ─→ adr
   └──────────────→ adr
```

A genuine decision discovered while executing work becomes a **new** ADR that supersedes the previous one. Neither an ADR nor a spec is ever rewritten to hide its history.

### ADRs: Append & Supersede, Never Overwrite
* **Immutable history**: the decision content of old ADRs is never deleted or rewritten.
* **Explicit supersession marking**:
<pre>
status: Superseded
superseded_by: ADR-XXXX
</pre>
* **Periodic consolidation**: all Accepted ADRs should be reflected in the core context file.

### Specs: Single Upper Layer
* **One spec per feature**: the authoritative statement of intent (problem, solution, user stories, acceptance).
* **Not archived on completion**: a spec remains the reference for its feature; it is not moved to the archive.
* **Decomposes into tickets**: a spec is the source from which tickets are cut. Reference the constraining ADRs rather than restating their reasoning.

### Tickets: Tracer Bullets, Archive & Knowledge Extraction
* **One independently verifiable unit each**: a ticket should be vertically deliverable and observable on its own, not a horizontal slice of several features.
* **Declare blocking edges**: record what a ticket is `blocked_by`, so ordering is derivable rather than remembered.
* **Extract knowledge on close**: promote durable knowledge into the docs or the core context.
* **Archive raw tickets**: prevent dilution of the active context.

### Session Summaries: Rolling GC
* **Single active file**: maintain a single active session state file (e.g., `.context/current-session.md`).
* **GC after completion**: after work is merged, clear or delete the session summary.
* **Keep only TODO items**: strip completed tasks, keep only:
  * Remaining TODOs
  * Key constraints
  * Blocking issues

---

## 2. Standard Document Structure

Recommended generic documentation directory structure:

<pre>
project/
├── .context/               # current session state (suggested for .gitignore)
│   └── current-session.md  # only unfinished tasks and constraints
├── docs/
│   ├── CONTEXT.md          # single source of truth (consolidates core context and Accepted ADRs)
│   ├── DEFINITION.md       # ubiquitous language / domain vocabulary (optional)
│   ├── DOCUMENTATION-LIFECYCLE.md
│   ├── CHANGELOG.md        # version history and milestone summaries
│   ├── spec/               # feature specs / PRDs (intent) — not archived
│   ├── adr/                # architecture decision records (never deleted, marked Superseded)
│   ├── ticket/             # tracer-bullet work units (checklists live here, not in ADRs)
│   ├── standards/          # development and technical standards
│   └── archive/            # archive directory (suggested for .gitignore)
│       ├── summaries/      # completed session summaries
│       └── tickets/        # completed tickets
└── CHANGELOG.md            # external version history
</pre>

### File naming and header metadata

* **File names** follow `NNNN-slug.md`. Numbering is an **independent sequence per kind** (a `spec` number and an `adr` number are unrelated).
* **Header metadata** per kind, so cross-references stay traceable:

<pre>
spec/NNNN    Status · Date · References (-> ADRs)
adr/NNNN     Status (Proposed|Accepted|Superseded) · Date · superseded_by · References (-> spec)
ticket/NNNN  Status (Todo|Doing|Done) · blocked_by · References (-> spec, -> ADRs) · Labels
</pre>

---

## 3. Workflow Checklist

Documentation hygiene checklist when performing work:

* [ ] **Before starting**: read the core context (e.g., `CONTEXT.md`) to understand the current system constraints.
* [ ] **Feature work**:
  * [ ] Ensure a spec exists describing the intent and acceptance.
  * [ ] Cut the spec into tickets, each declaring its blocking edges.
  * [ ] Reference the relevant ADRs; do not duplicate their reasoning.
* [ ] **Architecture changes**:
  * [ ] Create a new ADR (`docs/adr/NNNN-slug.md`).
  * [ ] Mark the old ADR as `Superseded by ADR-XXXX`.
  * [ ] Update the core context.
* [ ] **Ending a session/sprint**:
  * [ ] Strip completed items from the active session file.
  * [ ] If merged, run GC (clear or delete).
  * [ ] Update `CHANGELOG.md`.
* [ ] **Closing a ticket**:
  * [ ] Extract durable knowledge into the docs or the core context.
  * [ ] Archive into the history area.

---

## 4. AI Assistant Guidance

When working as an AI assistant:

1. **Read first**: the core context file (not all historical ADRs).
2. **Intent**: read the relevant spec to understand *what* is being built.
3. **Work**: pick up tickets; respect their `blocked_by` edges; a ticket references a spec/ADR rather than restating it.
4. **Architecture decisions**: check the Accepted ADRs in the architecture decision records.
5. **History tracing**: consult Superseded ADRs only when you need to know "why" a decision was made.
6. **Session records**:
   * Keep the active session state file up to date.
   * Run GC after tasks complete.
7. **Avoid**:
   * Creating scattered new session summary files.
   * Accumulating stale temporary files in the docs directory.
   * Duplicating immutable decisions (ADR) inside specs or tickets.
   * Treating implementation details as permanent documentation.
