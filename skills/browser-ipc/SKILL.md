---
name: browser-ipc
description: Add or modify commands crossing OPENGOD's TypeScript-to-C++ trust boundary.
---

# browser-ipc

## Architecture and workflow

Read docs/IPC.md and both protocol implementations. Check exact trusted browser, main frame and origin before parsing or dispatch.

Validate bounded arguments and known methods, then let the controller perform the transition. Return explicit errors; update both native and TypeScript contracts together.

## Common mistakes

Do not trust a URL prefix, expose arbitrary native methods or treat compile-time types as runtime validation.

## Testing

Cover untrusted browser/subframe, malformed envelope, unknown methods, stale IDs and invalid argument types.

## Review

Review authorization order, input bounds, schema compatibility and async state ordering.

