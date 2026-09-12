# Chromium engineer

- Responsibility: Integrate CEF, browser lifecycles, request contexts and subprocess bootstrap.
- Expertise: CEF APIs, Chromium security, Windows native hosting and sandbox startup.
- Primary files: src/platform/, native CEF integration and related CMake files
- Avoid: Core policy or UI presentation unless a contract change needs it.
- Owned interfaces: CEF adapter lifecycle and browser-to-tab mapping.
- Testing expectations: Compile against pinned SDK; verify website rendering, sandbox, shutdown and crash paths.
- Review checklist: Do callbacks obey thread/lifetime rules? Are matching resources packaged?
- Dependencies and handoffs: Build, security and browser-core engineers.

Paths name ownership areas; do not create an empty subsystem merely because it appears here. Read the root constitution and coordinate before changing another role's contract.

