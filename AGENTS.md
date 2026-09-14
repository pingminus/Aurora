# OPENGOD engineering constitution

## Architecture and ownership

Chromium provides Blink, V8, the network stack, compositing and sandboxing through CEF. C++20 owns durable browser state, tab identity, navigation policy and command validation. TypeScript owns presentation, focus, layout and animation. Keep CEF dependencies in the native integration layer so the core remains independently buildable and testable. Do not recreate web-platform behavior already supplied by Chromium.

`src/` contains the native application and core; `ui/` contains the HTML/TypeScript/CSS shell; `tests/` verifies native invariants; `tools/` contains reproducible build and verification utilities; `docs/` records decisions and status. Create additional modules only when implementing a real behavior. `agents/` assigns specialist review responsibility; `skills/` provides focused workflows. The lead architect coordinates interface changes and prevents overlapping work.

## Build and verification

See `docs/BUILD.md` for prerequisites and complete commands. The standalone sequence is `cmake -S . -B build/core`, `cmake --build build/core --config Release`, then `ctest --test-dir build/core -C Release --output-on-failure`. Native builds opt in using `OPENGOD_BUILD_BROWSER=ON` and `CEF_ROOT`. UI build/typecheck commands live in the root `package.json`; execute the existing scripts rather than inventing commands. Report separately: core tests, UI checks, native compilation, real website rendering and packaged launch.

## Coding conventions

Use `.clang-format` for C++, RAII and explicit ownership; avoid raw owning pointers and mutable global state. Keep public interfaces small. Use strong tab identifiers and return explicit failures for invalid transitions. Enable warnings; fix warnings rather than suppressing them broadly. Use strict TypeScript, typed command envelopes and unknown-to-validated narrowing at external boundaries. Avoid `any`, unsafe HTML insertion and UI-owned copies of native business policy. CSS uses shared tokens, semantic component names, responsive sizing and reduced-motion overrides. Use coherent vector icons rather than unrelated icon sets.

## Feature and state changes

Implement a user-visible vertical slice: define the command and state invariant, implement the C++ transition, connect the native effect, publish the state and render it in the UI. Add regression coverage for the invariant and failure path. CEF callbacks are asynchronous: verify tab identity and lifetime before applying them. Respect thread affinity and do not retain stale native browser handles. Presentation state may be local; authoritative tab state may only change through the controller. Keep the protocol and `docs/IPC.md` synchronized whenever a command changes.

## Security and dependencies

Treat frontend messages as untrusted. Authorize the exact trusted browser, main frame and internal origin before parsing and dispatching commands; web tabs must have no privileged bridge. Bound input sizes and validate argument types and navigation schemes. Preserve CEF sandbox, certificate checks and web security. Do not add production bypass flags. Deny unsupported permissions, downloads and external protocols until a complete policy exists. Never log visited URLs, tokens or page contents by default.

Add dependencies only after checking whether CEF or the standard library already solves the problem. Record source, version, license, maintenance cost and security implications in `docs/DECISIONS.md`. Keep dependency archives and generated output out of Git. A CEF upgrade requires API, sandbox, subprocess, resources and packaging verification together.

## Product quality

Use the liquid-glass tokens in the UI; retain an opaque readable fallback and constrain blur. Animate transform and opacity where practical. Keep native work off the UI thread when it can block. Measure startup, tab latency, frame time and memory before making performance claims. Every control must perform its named action or be explicitly unavailable; never simulate browsing success.

Provide semantic labels, visible focus, keyboard access, scalable text, high-contrast support and reduced motion. Check focus across the native shell/web-content boundary. Test malformed IPC, tab lifecycle races and shutdown, not only happy paths. Browser-only behavior needs a native runtime check; a UI preview cannot establish it. Document incomplete features and exact verification gaps. Avoid empty modules, silent catches, broad exception swallowing and speculative abstractions. Update architecture, decisions and roadmap when boundaries or completion status change.
