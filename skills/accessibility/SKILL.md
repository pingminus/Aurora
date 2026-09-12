---
name: accessibility
description: Implement or verify AURORA keyboard, focus, screen-reader, contrast or motion accessibility.
---

# accessibility

## Architecture and workflow

Read docs/UI.md. Native website focus and shell DOM focus are different surfaces and both require verification.

Use semantic controls, explicit labels, selected-tab semantics, visible focus and predictable dismissal/focus return. Preserve scalable text and reduced motion.

## Common mistakes

Do not assume DOM shortcuts run while web content is focused or use color alone to signal state.

## Testing

Exercise keyboard-only workflows, zoom/high DPI, high contrast and reduced motion; check the native app with Windows accessibility tooling.

## Review

Review reachability, focus order, announcements, target size and disabled-state clarity.

