---
name: browser-security
description: Implement or review AURORA navigation, permission, resource or bridge security controls.
---

# browser-security

## Architecture and workflow

Read docs/SECURITY.md and the actual CEF callbacks. Keep Chromium protections enabled and unsupported capabilities denied.

Define the capability and attacker-controlled inputs. Enforce policy in the browser process, constrain internal resource paths and avoid sensitive logs.

## Common mistakes

Do not add insecure launch switches, bypass certificate failures, trust internal-looking URLs or grant capabilities through frontend-only checks.

## Testing

Test negative cases from real web content where relevant, including subframes and external schemes; verify sandboxed native launch.

## Review

Review isolation, authorization, fail-closed errors, profile boundaries and observability privacy.

