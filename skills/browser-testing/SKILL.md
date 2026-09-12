---
name: browser-testing
description: Add or execute AURORA core, UI or native integration verification.
---

# browser-testing

## Architecture and workflow

Read docs/TESTING.md and existing test targets. Match test scope to the changed behavior and keep evidence by layer.

Assert externally observable invariants and failure behavior; make Release checks execute without relying only on assert. Reproduce races with deterministic sequences where practical.

## Common mistakes

Do not infer website rendering from core tests, match implementation text instead of behavior or hide missing runtime checks.

## Testing

Run the relevant CTest/npm commands; native changes require the actual application scenarios documented in TESTING.md.

## Review

Review false positives, meaningful negative cases, isolation and honest reporting of untested gates.

