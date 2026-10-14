# Documentation Lifecycle Management

This document describes the documentation lifecycle management mechanism, following the **concentrate** principles.

---

## 1. Principles Overview

### ADRs: Append & Supersede, Never Overwrite
* **Immutable history**: the decision content of old ADRs is never deleted or rewritten.
* **Explicit supersession marking**:
<pre>
status: Superseded
superseded_by: ADR-XXXX
</pre>
* **Periodic consolidation**: all Accepted ADRs should be reflected in the core context file.

### Session Summaries: Rolling GC
* **Single active file**: maintain a single active session state file (e.g., `.context/current-session.md`).
* **GC after completion**: after work is merged, clear or delete the session summary.
* **Keep only TODO items**: strip completed tasks, keep only:
  * Remaining TODOs
  * Key constraints
  * Blocking issues

### Tickets: Archive & Knowledge Extraction
* **Extract knowledge on close**: promote durable knowledge into the docs or the core context.
* **Milestone summaries**: at the end of each sprint/release, consolidate into the version history (`CHANGELOG.md`).
* **Archive raw tickets**: prevent dilution of the active context.

---

## 2. Standard Document Structure

Recommended generic documentation directory structure:

<pre>
project/
├── .context/               # current session state (suggested for .gitignore)
│   └── current-session.md  # only unfinished tasks and constraints
├── docs/
│   ├── CONTEXT.md          # single source of truth (consolidates core context and Accepted ADRs)
│   ├── CHANGELOG.md        # version history and milestone summaries
│   ├── adr/                # architecture decision records (never deleted, marked Superseded)
│   ├── ticket/             # specs/issues (to-spec format); checklists live here, not in ADRs
│   ├── standards/          # development and technical standards
│   └── archive/            # archive directory (suggested for .gitignore)
│       ├── summaries/      # completed session summaries
│       └── tickets/        # completed tickets
└── CHANGELOG.md            # external version history
</pre>

---

## 3. Workflow Checklist

Documentation hygiene checklist when performing code tasks:

* [ ] **Before starting**: read the core context (e.g., `CONTEXT.md`) to understand the current system constraints.
* [ ] **Architecture changes**:
  * [ ] Create a new ADR (`docs/adr/XXXX.md`).
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
2. **Architecture decisions**: check the Accepted ADRs in the architecture decision records.
3. **History tracing**: consult Superseded ADRs only when you need to know "why" a decision was made.
4. **Session records**:
   * Keep the active session state file up to date.
   * Run GC after tasks complete.
5. **Avoid**:
   * Creating scattered new session summary files.
   * Accumulating stale temporary files in the docs directory.
   * Treating implementation details as permanent documentation.
