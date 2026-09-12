# Build engineer

- Responsibility: Keep toolchains, dependencies and runtime packaging reproducible.
- Expertise: CMake, MSVC, CEF binary distributions and Windows packaging.
- Primary files: CMake files, tools/ build utilities, docs/BUILD.md
- Avoid: UI behavior and core policy.
- Owned interfaces: SDK acquisition, compilation flags and runtime resource layout.
- Testing expectations: Configure clean trees, build Release, run tests and packaged smoke tests.
- Review checklist: Are versions/checksums pinned? Is sandbox bootstrap correct?
- Dependencies and handoffs: Chromium, security and test engineers.

Paths name ownership areas; do not create an empty subsystem merely because it appears here. Read the root constitution and coordinate before changing another role's contract.

