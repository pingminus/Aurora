# Architecture decisions

## ADR 001 — CEF integration

Use the Chromium Embedded Framework binary SDK for the first native Windows product slice. The repository started empty; a Chromium fork would add disproportionate build and maintenance costs before proving OPENGOD's product boundaries. Keep CEF behind native adapters. Revisit this choice when a required Chromium capability lacks a secure CEF extension point, documenting the specific API gap first.

Dependency source: the [CEF project](https://bitbucket.org/chromiumembedded/cef) and its [binary distributions](https://cef-builds.spotifycdn.com/index.html). CEF uses a BSD license; Chromium and bundled components carry additional notices that must accompany redistribution. Pin the exact SDK version and archive checksum in build tooling. No distribution license audit or production security certification is implied.

## ADR 002 — Native core, web presentation

Use C++20 for state and policy, TypeScript/HTML/CSS for the shell. The core builds without CEF to keep transition tests inexpensive and deterministic. The UI never owns the authoritative tab list or arbitrary native capabilities. Compile-time types complement runtime validation; they do not replace it.

## ADR 003 — Separate trusted and web surfaces

Host the privileged shell and untrusted web tabs as separate CEF browser instances in native child windows. Avoid a remote website iframe inside privileged chrome. Validate browser identity, frame and origin before dispatching every shell request. Navigation to an internal-looking URL does not grant privileges. Native child surfaces limit CSS compositing across web content; do not promise glass effects that cross this boundary.

## ADR 004 — Fail closed during feature growth

Unsupported permissions, downloads and external schemes remain denied until a complete policy and interface exist. Sandbox support is mandatory. No insecure launch mode ships to make setup easier. Release readiness includes packaging, update strategy and clean-machine execution, not only compilation.

## ADR 005 — Dependencies and scope

Prefer the standard library and CEF capabilities over a second networking stack or renderer. Use a TypeScript compiler for typed UI source; evaluate further UI libraries only when they reduce demonstrated complexity. Durable storage will need an explicit database decision with migrations and profile isolation. Extensions and AI are future product work rather than empty interfaces in the initial slice.

## ADR 006 — Native per-tab audio and custom frame

CEF 152 exposes mute and PCM capture but no per-tab volume setter. Use its Alloy capture path with one C++ XAudio2 output per tab; mute Chromium playback to prevent bypass and apply stored native gain to captured PCM. No injected page script or process-wide session volume is used. XAudio2 and Comctl32 are Windows SDK/system components governed by the installed Microsoft SDK terms; no additional redistributable dependency is vendored. Maintenance includes buffer ownership, callback shutdown, device failures and latency; current limits and measured evidence are in AUDIO.md.

The custom HTML title bar uses CEF draggable-region callbacks plus Win32 child/root hit testing, following the pinned CEF cefclient pattern. Each subclass owns its copied region until WM_NCDESTROY. Only the privileged shell supplies regions; website callbacks cannot alter the frame. Windows owns resize/move behavior and monitor geometry.

## ADR 007 — NVD feed and narrow new-tab capability

Use existing CEF networking and JSON APIs with C++20 date/filter/score/cache policy. No network or JSON dependency is added. NVD API 2.0 is an external public data service, subject to NVD terms and rate limits; the UI includes the requested NVD non-endorsement notice. A new-tab main frame receives only narrowly validated home-page capabilities, never the shell's browser commands. The user explicitly authorized an exception to the old no-newtab-bridge rule. The default theme is charcoal/violet dark glass with opaque and reduced-transparency fallbacks.

## ADR 008 — Opt-in session-cookie retention

The home tab saves a per-user `HKCU\Software\OpenGod\SaveLoginSessions` DWORD, defaulting to off. At startup, off leaves CEF's global `cache_path` empty, creating an in-memory default profile that does not persist profile-specific website data. On selects the per-user `OpenGod/Profiles/Default` cache path and enables `persist_session_cookies`, retaining website data and cookies without an expiry date across normal restarts. CEF reads these settings during initialization, so a changed preference requires a restart. Turning the switch off does not delete data saved during an earlier on-mode session. Keep Chromium's cookie rules, expiry, certificate checks and sandbox behavior; do not copy credentials into OpenGod's own state or logs. This does not restore tabs or provide isolated named profiles.

## Product identity — 2026-09-14

OpenGod is the product name. Native targets and namespaces, internal origins, IPC events, package names and profile keys use the new identity. The checkout directory stays unchanged. Settings and persistent website data use a fresh OpenGod namespace; existing data from earlier branding is neither migrated nor deleted. The open-ring G and star logo is an original repository-owned vector, with a multi-resolution Windows icon generated by tools/build-icon.ps1. Packaging applies icon resources to the copied CEF bootstrap only, before any future signing step, without modifying its sandbox entry point or the SDK.
