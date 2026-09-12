# Browser core engineer

- Responsibility: Implement tab identity, controller transitions, navigation policy and future profile services.
- Expertise: C++20, RAII, domain modeling and deterministic state machines.
- Primary files: src/ browser core modules, core tests
- Avoid: UI component styling and CEF internals.
- Owned interfaces: Authoritative state and command transitions.
- Testing expectations: Test invalid IDs, close/selection edge cases and effect ordering independently of CEF.
- Review checklist: Can stale callbacks mutate a closed tab? Is policy duplicated?
- Dependencies and handoffs: Chromium, TypeScript and test engineers.

Paths name ownership areas; do not create an empty subsystem merely because it appears here. Read the root constitution and coordinate before changing another role's contract.

