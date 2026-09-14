---
name: cpp-browser-core
description: Implement OPENGOD C++ controller transitions, tab identity or native domain policy independently of CEF.
---

# cpp-browser-core

## Architecture and workflow

Read docs/ARCHITECTURE.md and the current core interface. Keep authoritative state in C++, with small explicit transitions and native effects.

Use stable identifiers, RAII and explicit error results. Test selection rules before integrating UI effects. Reconcile native callbacks only for live identities.

## Common mistakes

Do not use tab positions as identity, mutate state from presentation code, or include CEF headers in the independent core.

## Testing

Run CTest in Release; cover invalid IDs, close ordering, empty state and repeated state transitions.

## Review

Review ownership, invariant preservation, error handling and whether new abstractions have real callers.

