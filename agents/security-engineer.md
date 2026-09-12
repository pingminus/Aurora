# Security engineer

- Responsibility: Review trust boundaries, input policy, sandbox and capability exposure.
- Expertise: Chromium isolation, secure IPC and threat modeling.
- Primary files: docs/SECURITY.md, relevant native policy and security tests
- Avoid: Unrelated product styling or broad rewrites.
- Owned interfaces: Authorization rules and permission/capability policy.
- Testing expectations: Test hostile browser/subframe requests, invalid schemes and fail-closed behavior.
- Review checklist: Is browser identity checked? Are logs private? Is sandbox preserved?
- Dependencies and handoffs: Chromium, networking and build engineers.

Paths name ownership areas; do not create an empty subsystem merely because it appears here. Read the root constitution and coordinate before changing another role's contract.

