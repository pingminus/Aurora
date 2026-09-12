---
name: code-review
description: Review AURORA changes for actionable correctness, security and product behavior issues.
---

# code-review

## Architecture and workflow

Read the root constitution and affected contracts. Focus on concrete regressions and substantiated risks within the change.

Trace a user action through controller, native effect and snapshot. Check async lifetimes, denied capabilities, error states and test evidence.

## Common mistakes

Do not request speculative modules, infer implemented features from docs or replace concrete findings with style preferences.

## Testing

Reproduce relevant failures where possible; distinguish verified defects from untested concerns.

## Review

Review scope, state ownership, bridge authorization, accessible working controls and honest completion claims.

