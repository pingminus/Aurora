# OpenGod — STILL IN PROGRESS

A C++20 browser platform using Chromium through CEF, with a TypeScript, HTML and CSS browser interface. OPENGOD's core owns browser state; the frontend presents it through a validated command boundary.

![OpenGod logo](ui/icons/opengod.svg)

This repository is an early implementation, not a production browser. The initial target is Windows x64. Core builds do not require Chromium; the native browser requires an external CEF binary distribution and the Windows C++ toolchain. The native Windows Release build, core CTest suite and six UI tests pass. Native launch, new-tab rendering, MDN website rendering and basic create/switch/close tab interactions have been verified. Keyboard focus has a known issue, and adversarial runtime IPC checks remain pending; this is not release-ready.

Start with [build instructions](docs/BUILD.md), [architecture](docs/ARCHITECTURE.md) and the [roadmap](docs/ROADMAP.md). See [testing](docs/TESTING.md) for evidence required before claiming a working browser and [security](docs/SECURITY.md) for the trust boundaries.

The first vertical slice is a native window containing a trusted browser interface and separate CEF web content, with tab creation, navigation, switching and closing. Full profiles, persistent history and bookmarks, workspaces, downloads, extensions and release infrastructure are tracked as later milestones; their appearance in the product vision does not mean they are implemented.

Engineering guidance lives in [AGENTS.md](AGENTS.md), specialized responsibilities in [agents/](agents/) and reusable project playbooks in [skills/](skills/).
