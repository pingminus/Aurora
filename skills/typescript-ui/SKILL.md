---
name: typescript-ui
description: Build AURORA shell components or typed frontend state synchronization.
---

# typescript-ui

## Architecture and workflow

Read docs/UI.md and docs/IPC.md. The root package.json defines UI scripts. Native snapshots are authoritative; frontend state handles presentation.

Use strict types, validate external responses, render untrusted strings as text and serialize or correlate asynchronous requests.

## Common mistakes

Do not fake bridge success in preview, use any to bypass schema checks or duplicate native navigation policy.

## Testing

Run npm test and npm run typecheck from the root; check command errors and stale responses.

## Review

Review component boundaries, working controls, safe DOM updates and focus behavior.

