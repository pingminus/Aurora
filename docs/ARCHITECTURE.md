# Architecture and initial assessment

## Reconnaissance

The initial workspace was an empty Git repository on Windows, not a Chromium checkout or an existing CEF application. There was no existing source architecture, dependency graph, test suite or UI to preserve. Reconnaissance found CMake 4.0.1, Node.js 24.20.0, Visual Studio installations and MinGW. CEF was not initially installed; obtaining an SDK and validating the matching Windows toolchain are explicit native-build prerequisites. The initial SDK acquisition pins CEF 152.0.6 with Chromium 152.0.7977.83; see `tools/fetch-cef.ps1`. Native configuration subsequently succeeded with MSVC 19.44.35222 and the Visual Studio 2022 generator. This initial assessment does not itself claim successful runtime verification.

## Platform choice

Use CEF as the Chromium integration boundary. This permits a native C++ product without maintaining a Chromium fork or downloading and building the entire source tree before the first vertical slice. CEF supplies the rendering platform, not a complete browser product: AURORA must own tabs, policies, state, storage and interface behavior. APIs requiring deeper Chromium integration must be justified individually in the decision log.

```text
TypeScript shell: components, layout, focus, animation
           | validated commands / authoritative snapshots
C++ controller: tabs, identity, navigation policy, transitions
           | native effects and CEF lifecycle callbacks
Windows host + CEF: native children, browser instances, subprocesses
           | Chromium
      Blink / V8 / network / compositor / sandbox
```

The trusted shell and web content occupy separate native child surfaces. Websites are real CEF browser instances, not iframes inside the privileged UI. The shell uses a private internal origin, restrictive content security policy and an allowlisted command bridge. Authorization requires the trusted browser identity, main-frame identity and expected URL. A matching URL alone is insufficient.

## State and lifecycle

The core is independent of CEF. It owns stable tab identifiers, selection and allowed navigation transitions. The integration layer maps IDs to CEF instances and applies controller effects. Async callbacks must tolerate closing tabs and stale references. UI state is a rendered snapshot, not the source of truth. Native events such as navigation commit and title changes feed back into the controller.

Closing the native window initiates browser closure and waits for browser lifecycle completion before CEF shutdown. Renderer crashes are per-tab conditions; recovery must preserve the controller's invariants. Profile-aware durable storage is a later milestone and must not be inferred from transient state support.

## Module growth

Add profile, storage, history, bookmark, download and permission services only with real implementations and tests. Persist profile data through a versioned storage boundary with migrations and crash recovery. Introduce an extension or AI interface only when a concrete feature needs it; optional remote AI must require explicit user choice and must not receive browsing data by default.

The initial Windows host is platform-specific. Core portability does not imply a macOS or Linux browser host exists. Multi-window support, native accessibility integration and Chromium extension compatibility require separate implementation and validation.

## Weekly vulnerability feed

The CEF adapter owns a shared reference-counted CVE service, independent of tab lifetimes. Its request callbacks carry generation IDs instead of comparing CEF wrapper pointers. CEF background tasks own network and parsing work; the UI thread reads a mutex-protected serialized snapshot. Pure core policy supplies UTC week boundaries, metric precedence, sorting, page consistency and memory-cache transitions. The starting page is authorized only for the narrowly scoped read-only feed protocol documented in IPC.md.
