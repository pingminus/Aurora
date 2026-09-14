# Product roadmap

This is a sequence of verifiable vertical slices. Source code, compilation and native runtime verification are distinct completion gates. As of 2026-09-10, the native Windows Release build, core CTest suite and six UI tests pass. Native sandbox-bootstrap launch, new-tab rendering, real MDN navigation with title synchronization, tab creation/switching and background-tab closing have been visually verified at 1360 × 900. The first-browser gate remains open: adversarial runtime IPC checks are pending and Ctrl+L currently reaches the hidden CEF omnibox instead of the OPENGOD address bar.

| Milestone | Status | Owner | Dependencies | Definition of done | Required tests |
| --- | --- | --- | --- | --- | --- |
| Foundation | Implemented; build/tests pass | Lead architect / build engineer | C++20, CMake, Node, pinned CEF SDK | Independent core, typed shell, reproducible native build and project guidance | Core CTest; UI six tests and typecheck; native Release compile |
| First browser | Basic native runtime verified; hostile IPC gate pending | Chromium / UI engineers | Foundation | Sandboxed native window renders a real website; create, switch and close tabs; validated C++/TS bridge | Native launch/render; tab lifecycle; hostile bridge requests; clean shutdown |
| Reliable lifecycle | Basic tab lifecycle verified; keyboard focus defect; stress/crash gates pending | Chromium / test engineers | First browser | Back/forward/reload, keyboard focus, renderer recovery and shutdown work under stress | Repeated open/close; renderer crash; shutdown while loading; no orphan processes |
| Durable personal browser | Website-session saving is opt-in and restart-gated through CEF's default profile; broader milestone planned | Browser-core / security engineers | Reliable lifecycle; storage design | Isolated profiles, tabs, settings, history and bookmarks survive restart safely | Restart; corruption recovery; migrations; cross-profile isolation |
| Downloads and permissions | Planned; unsupported actions denied | Security / networking engineers | Profile/storage boundary | Origin-bound grants and complete download progress/cancel/retry workflows | Unsafe filenames; denied origins; revocation; interrupted downloads |
| Workspaces | Core transitions implemented; product integration/persistence planned | Browser-core / UI engineers | Durable personal browser | Persistent workspaces, groups, pinned tabs, reopen/duplicate and search | Core invariants; UI/native synchronization; restart restoration |
| Advanced interaction | Planned | UI / UX engineers | Reliable lifecycle; workspaces | Split views, drag/drop, mute, suspend, previews and categorized omnibox | Keyboard/pointer paths; suspension recovery; measured memory impact |
| Developer tools | DevTools command implemented; runtime verification pending; broader diagnostics planned | Chromium / performance engineers | First browser | Working DevTools plus accurate network/storage/process diagnostics | Native DevTools lifecycle; measurement accuracy; sensitive-log checks |
| Release hardening | Planned | Build / test / security engineers | Prior browser milestones | Repeatable benchmarks, accessible packaged app, signing and update strategy | Clean-machine packaged smoke test; accessibility; dependency audit; regression baselines |

The C++ core includes transient workspace/pin/mute transitions, but these are not claims of a complete native feature or durable product workflow. Full profiles, history/bookmark databases, download manager, advanced tabs, extension ecosystem and optional AI remain future work. Unsupported controls must not masquerade as working features.

Reserve internal product routes for new tab, settings, history, bookmarks, downloads, workspaces, about, flags and diagnostics. Introduce each route alongside its actual implementation. `about:opengod` may eventually alias the about route; do not show fabricated diagnostics.

### Bookmark bar status (2026-09-13)

A session-lifetime bookmark collection and a bookmark bar below the navigation row are implemented: star-to-save from the active tab, idempotent add by URL, remove from the bar, and click-to-navigate. The chrome is now 140 CSS pixels (48 tab row + 64 navigation + 28 bookmark bar). Bookmarks are not persisted and are not part of the durable-profile milestone.

### Audio/frame status (2026-09-12)

Implemented native per-tab mute/volume, accessible shell controls and custom title bar. Two-tab PCM output isolation is verified. Final drag/resize check, forced renderer recovery, video synchronization and device-loss recovery remain open; see AUDIO.md and TESTING.md. This does not complete session persistence or the broader browser roadmap.

## Product identity — 2026-09-14

OpenGod naming and the new vector/Windows logo are implemented across the native host, trusted routes, shell, packaging and project documentation. Core/UI checks, native Release compilation, packaged launch and MDN rendering pass. Persistent settings use the new product namespace; automatic migration from previous branding is outside this change.
