---
name: build-and-release
description: Modify OPENGOD dependency acquisition, CMake, Windows packaging or release verification.
---

# build-and-release

## Architecture and workflow

Read docs/BUILD.md, docs/DECISIONS.md and the current acquisition scripts. Use the exact pinned SDK and compatible toolchain.

Keep core-only builds independent. Package matching CEF resources and the sandbox-compatible bootstrap; record build inputs and third-party notices.

## Common mistakes

Do not commit SDK archives, mix resource revisions, suppress warnings or treat build success as release readiness.

## Testing

Configure a clean tree, build Release, run core/UI checks and test the packaged executable on a clean environment before release claims.

## Review

Review checksums, compiler compatibility, sandbox, resource completeness, signing/update scope and license notices.

