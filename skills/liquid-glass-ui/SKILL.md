---
name: liquid-glass-ui
description: Change OPENGOD's glass design tokens, icons, responsive surfaces or motion.
---

# liquid-glass-ui

## Architecture and workflow

Read docs/UI.md. Respect separate native shell/web surfaces; CSS blur cannot cross that composition boundary.

Use shared tokens and one vector icon language. Limit blur area and layer count; provide readable opaque, reduced-motion and high-contrast variants.

## Common mistakes

Avoid hard-coded theme colors, continuous expensive animations and effects that require website iframe embedding.

## Testing

Inspect light/dark/system, small windows, high DPI and reduced motion; measure frames for substantial animation changes.

## Review

Review readability, focus contrast, token reuse and compositing cost.

