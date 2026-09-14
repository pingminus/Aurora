---
name: chromium-integration
description: Integrate or upgrade OPENGOD's CEF adapter, native host or Chromium dependency.
---

# chromium-integration

## Architecture and workflow

Read docs/ARCHITECTURE.md, docs/BUILD.md and the selected CEF SDK headers. Preserve the mandatory sandbox bootstrap and matching resources. Keep CEF handles out of the standalone core.

Map native browser IDs to stable core tab IDs; verify thread affinity and re-check lifetime in asynchronous callbacks. Shut down only after browser closure completes.

## Common mistakes

Do not copy APIs from another CEF revision, disable sandbox to fix startup, or implement a second renderer.

## Testing

Build against the pinned SDK; render a website; verify popup policy, renderer failure and no orphan subprocesses.

## Review

Review SDK/resource consistency, callback ownership, trusted-shell isolation and licensing implications.

