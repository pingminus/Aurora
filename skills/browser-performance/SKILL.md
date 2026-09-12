---
name: browser-performance
description: Measure or investigate AURORA startup, tab, IPC, memory or rendering performance.
---

# browser-performance

## Architecture and workflow

Read docs/PERFORMANCE.md. Distinguish core microbenchmarks, UI previews and native application measurements.

Use a fixed fixture, record environment and raw samples, compare distributions and trace the demonstrated bottleneck before optimizing.

## Common mistakes

Do not present a single best run as a baseline, include only the parent process in memory totals or log browsing data.

## Testing

Repeat the benchmark with identical inputs; validate functional behavior after any optimization.

## Review

Review measurement definitions, noise, subprocess accounting and regression reproducibility.

