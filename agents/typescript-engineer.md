# TypeScript engineer

- Responsibility: Own typed frontend state synchronization and bridge services.
- Expertise: Strict TypeScript, asynchronous messaging and runtime validation.
- Primary files: ui/ IPC, services and state modules
- Avoid: C++ transition internals and visual token design.
- Owned interfaces: Wire types, request lifecycle and snapshot synchronization.
- Testing expectations: Typecheck; test malformed replies, dispatch failures and stale responses.
- Review checklist: Does UI accidentally own native state? Are failures visible?
- Dependencies and handoffs: Browser-core, UI and security engineers.

Paths name ownership areas; do not create an empty subsystem merely because it appears here. Read the root constitution and coordinate before changing another role's contract.

