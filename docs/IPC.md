# Shell/native protocol

The dedicated shell uses CEF's message router to call an allowlisted browser-process command dispatcher. The C++ dispatcher is authoritative. The frontend service owns request correlation and transforms validated responses into presentation state.

## Request lifecycle

1. The frontend constructs a typed command with its arguments.
2. Native code checks trusted browser identity, main-frame identity and the exact internal origin.
3. The parser validates the envelope, bounded string lengths, known method and argument types.
4. The controller checks tab identity and state invariants, then produces the native effect.
5. The response returns an updated authoritative snapshot or an explicit error. CEF navigation callbacks update later loading, title and URL state.

Consult the actual dispatcher and TypeScript protocol definitions for exact supported method names and wire fields. Additions must change both sides together. Do not infer support from a roadmap command name. Unknown methods fail explicitly; no generic native reflection, shell execution, arbitrary filesystem API or JavaScript evaluation capability is exposed.

## Invariants

Tab identifiers are stable native identities, not array positions. Closing a tab invalidates subsequent operations on its identifier. Selected-tab state always refers to an existing tab or an explicit empty state. Navigation is a requested effect, not evidence that a page loaded successfully. Async callbacks must tolerate a tab closing between request and completion.

Treat every response as external data in TypeScript. Report dispatch errors without presenting a successful state transition. Avoid overlapping snapshot requests that can overwrite newer state with older responses; correlate or serialize as appropriate. A UI preview can show bridge-unavailable status, but must not synthesize native success.

## Change checklist

Define argument/result types and limits; add native validation and controller tests; connect the effect; update frontend types and rendering; test unknown IDs, malformed arguments, stale callbacks and bridge access from an untrusted browser. For an incompatible schema change, introduce an explicit version negotiation or update all packaged components atomically.

## Current version 1 surface

The shell URL is exactly `aurora://shell/index.html`. Requests are JSON objects with required `version: 1` and `command`, optional integer `tabId`, optional string `url`, command-specific `muted`, `volume` or `bookmarkId`. Unknown fields are rejected. Nonpositive IDs, persistent queries and requests over 16,384 characters are rejected. No response stream is opened.

Supported command names are `state`, `createTab`, `activateTab`, `closeTab`, `duplicateTab`, `reopenTab`, `navigate`, `back`, `forward`, `reload` `devtools`, `minimizeWindow`, `toggleMaximize`, `closeWindow`, `setTabMuted`, `setTabVolume`, `addBookmark` and `removeBookmark`. Omitting `tabId` targets the active tab for tab operations. `createTab` defaults to `aurora://newtab`; `navigate` requires `url`. CEF message-router success returns the snapshot JSON; failure returns an error code/message.

Snapshots contain `version`, `activeTab`, `tabs` and `bookmarks`; each tab contains `id`, `url`, `title`, `active`, boolean `muted` and integer `volume` (0–100), with optional string `audioError`. Each bookmark contains `id`, `title` and `url`. Frontend validation checks identity uniqueness and active-tab consistency. The native wire currently uses CEF integer IDs even though the core uses 64-bit identities; expanding the ID range requires an explicit protocol update.
## Per-tab audio and window frame

`setTabMuted` requires explicit positive integer `tabId` and boolean `muted`. `setTabVolume` requires explicit positive integer `tabId` and integer `volume` from 0 through 100. Missing values, wrong types, fractions, out-of-range values, wrong-command audio fields and closed IDs fail without changing state. Example: `{"version":1,"command":"setTabVolume","tabId":2,"volume":37}`.

New and duplicated tabs start unmuted at 100. Mute changes effective output gain to zero without changing stored volume; changing volume while muted updates the value used on unmute. Navigation, reload and stream restart preserve the tab settings. Reopen restores saved settings under a new identity. C++ owns settings and playback; TypeScript only renders snapshots and requests mutations.

Snapshots additionally contain `window: {maximized: boolean}`. Window buttons send the three window commands above. Dragging uses CEF draggable regions and Win32 nonclient hit testing, not IPC. Only the trusted shell main frame may register draggable regions.

## Bookmarks

The shell renders a bookmark bar below the navigation row. `addBookmark` copies the target tab's URL and title (`tabId` optional; defaults to the active tab). Internal `aurora://` pages, terminal tabs and unknown tabs are rejected. Adding an already-saved URL is idempotent and returns the existing bookmark without creating a duplicate. `removeBookmark` requires a positive integer `bookmarkId`; unknown identifiers fail without changing state. Clicking a bookmark navigates the active tab through the existing `navigate` command.

The chrome is 140 CSS pixels tall (48 tab row + 64 navigation + 28 bookmark bar). Bookmarks are in-memory and last for the window lifetime; persistence belongs to the durable-profile milestone.

## Read-only starting-page CVE capability

The exact internal main-frame URLs aurora://newtab and aurora://newtab/ may send version 1 commands cveState and refreshCves, with no additional fields and a 256-character bound. The native Client checks its live tab/browser identity, main frame, exact URL, nonpersistent request and shutdown status before parsing. Web pages and internal subframes have no bridge. A starting-page request never enters the shell dispatcher.

The response contains version, status (loading/ready/stale/error), message, UTC start/end/fetched timestamps and entries. Each entry carries id, description, published, modified, nullable numeric score, cvssVersion, severity and assessmentSource. C++ owns filtering, ordering and cache policy; TypeScript validates and presents the snapshot in groups of 50. Navigating or closing a page cancels its frontend polling; shared native refreshes can finish for other tabs. See CVE-FEED.md for policy and limits.

CVE ordering now uses highest score first (descending), then CVE ID for ties, with unscored entries last, per the user’s clarification. The wire schema is unchanged.

The CVE feed now covers today: midnight UTC through the current fetch start time. Cache reuse is limited to the same UTC date; previous-day entries are cleared when a new-day refresh begins. Highest-score-first sorting and the wire schema are unchanged.

## New-tab focus

New browsing-view creation sends the trusted shell `aurora-new-tab-address`, with the native tab ID as its numeric detail. The shell refreshes validated state and focuses/selects the address only if that tab is still active and is not a terminal. Native CEF focus transfers to the shell; navigation-origin focus requests from the starting page are suppressed, while user focus requests remain allowed. Terminal creation does not send this event. Existing tab activation does not refocus the address.
