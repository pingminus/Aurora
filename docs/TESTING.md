# Verification strategy

## Independent layers

Core tests run through CTest and exercise observable invariants: stable IDs, selection after close, invalid operations, URL policy, bounded inputs and controller transitions. Run Release tests as well as debug builds; assertions alone are not a sufficient Release test mechanism.

UI verification starts with the declared typecheck/build scripts. Interaction checks cover address submission, tab selection/closing, command palette keyboard behavior, bridge failures, responsive layout, reduced motion and focus restoration. Static screenshots are useful for layout but cannot establish native correctness.

Native integration verification requires the built Windows application and matching CEF resources. Record exact commands and results; do not mark a test passed because its code exists.

## First-browser runtime gate

1. Launch the packaged executable with sandbox support and no security bypass flags.
2. Enter an HTTPS website and verify real content renders; inspect a title/URL update.
3. Create two tabs, navigate independently, switch and close them, including the selected and final tab.
4. Exercise back, forward and reload where exposed; confirm unsupported actions are unavailable.
5. Verify address-bar and palette shortcuts with focus in both shell and website.
6. Resize through small/medium/large windows and high-DPI scaling; inspect native child bounds.
7. Attempt bridge access from a website and subframe; verify rejection without state mutation.
8. Exercise unsupported schemes, certificate errors, popups, permissions and downloads against the documented policy.
9. Close the window during navigation; confirm no orphan browser subprocesses remain.

## Hardening gates

Renderer crash recovery, corrupted persisted data, restart restoration, profile separation, failed resource loading, accessibility tooling and clean-machine packaging require dedicated scenarios as those features mature. Use sanitizers where the toolchain supports them and run parser fuzzing when the command surface grows. Add regressions for real failures rather than tests that merely compare implementation text.

Report four separate outcomes: core verification, UI verification, native compilation and native runtime verification. A failure or untested layer stays visible in the implementation report.

## Verification record — 2026-09-10

The native Windows Release build, core CTest suite and six UI tests pass. The application launched using the CEF sandbox bootstrap. Visual native checks at 1360 × 900 confirmed the new-tab page, navigation via its MDN quick link to `https://developer.mozilla.org/en-US/`, actual website rendering and synchronized native tab title. Creating another tab, switching back to the preserved MDN page and closing the background tab succeeded.

A keyboard defect remains under investigation: Ctrl+L reaches the hidden CEF omnibox rather than the AURORA address bar. Keyboard acceptance is therefore not passed. Adversarial bridge requests from real web content/subframes have not yet been exercised. Stress/crash handling, clean shutdown under load, remaining policy scenarios and accessibility gates also need their dedicated checks; the first-browser milestone remains open.

## Audio/frame verification update — 2026-09-12

Fresh core Release rebuild and CTest passed (including 5,000 randomized transitions and audio regression checks). UI typecheck and all nine UI tests passed. Native Release compilation and sandbox-bootstrap launch passed with the custom frame and accessible per-tab controls. Two simultaneously playing website tabs passed process-scoped audio isolation, mute, change-while-muted, reload and closure checks; see AUDIO.md for values and limitations.

Custom maximize/restore buttons passed runtime checks on September 11. The final drag implementation uses native CEF regions instead of asynchronous IPC. It compiles and launches, but live drag/double-click/resize/minimize/close checks on this final implementation remain pending: user desktop input interrupted the September 12 check. Renderer-crash recovery, video A/V sync, full malformed native IPC, screen-reader operation, mixed-DPI monitors and clean-machine packaging remain open gates.

Final checks: the documented `build/browser` Release package rebuilt successfully without reported compiler/linker warnings, its core CTest passed, and the checked-in `tests/audio` lifecycle CTest passed on the default device. The spectrum estimator self-test also passed. The final package directory was built; clean-machine distribution launch is not verified.

## NVD timeout fix — 2026-09-12

Replaced CEF request-wrapper address comparisons with a dedicated per-request callback client and generation validation. The native Release package loaded 3,639 live NVD records for September 7 through September 12 after eight pages, completing at 15:57:54 UTC from a 15:56:59 UTC start. The actual Aurora accessibility tree reported complete results and ascending initial scores 0.9, 1.0, 1.1, 1.1, 1.2. Both standalone core CTest suites, packaged core CTest suites, UI typecheck and all twelve UI tests pass. The native build passed. This verifies successful real callbacks and pagination; forced late-callback/timeout fault injection and complete malformed-native-IPC coverage remain open.

The populated CVE cards also passed an actual window scroll check. The in-page feed shortcut uses scrollIntoView without a fragment navigation, preserving the exact URL required by the read-only bridge.

## Daily feed verification — 2026-09-12
Changed the feed to midnight UTC through now, with previous-day cache invalidation. Core regression checks cover midnight exclusion/inclusion, future records and next-day invalidation. Both standalone CTest suites and all twelve UI tests pass; the native Release build passes. Actual packaged Aurora loaded 80 CVEs with request interval ending 16:07:25 UTC and successful completion at 16:07:26 UTC (approximately one second for this run). Its first scores were 10.0, 9.9, 9.8. This measurement is a single live run, not a guaranteed response time. It supersedes weekly scope and ascending-order descriptions in earlier historical verification entries.
