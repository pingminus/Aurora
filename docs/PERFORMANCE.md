# Performance measurement

No production performance baseline is claimed yet. Measurements from standalone core code and a web UI preview do not establish full-browser startup, navigation or memory performance.

| Metric | Definition | Measurement context |
| --- | --- | --- |
| Cold/warm startup | Process launch to first usable shell and web surface | Release build, separate cold and warm runs |
| Tab creation | Command submission to usable native tab | Empty page and real page measured separately |
| Navigation | Request, commit and finished timestamps | Fixed local fixture; remote latency separately |
| Shell frame time | Frame duration and long tasks during interaction | Tabs, palette, resize, reduced motion |
| Memory/CPU/GPU | Browser plus all subprocesses | Idle, 1/10/50 tabs, foreground/background |
| IPC | Request to validated response | Payload size and concurrency recorded |
| Storage | Commit/search/recovery time | Once durable storage exists |

Benchmark utilities belong under `tools/benchmarks/`. Record OS, hardware, compiler/configuration, CEF revision, dataset, run count and raw samples. Prefer medians and tail percentiles over a single best run. Use a fixed local fixture to separate OPENGOD overhead from internet variability. Do not log real browsing data in benchmark artifacts.

Investigate regressions with traces before optimizing. Likely pressure points include excessive snapshot polling, offscreen tab activity, too many composited glass layers, blocking file access and native resize churn. Keep hidden-tab policy explicit and verify that throttling or suspension does not break audio, forms or tab recovery.

Set quantitative budgets only after collecting a repeatable baseline on declared reference hardware. CI thresholds should tolerate measured noise and preserve raw evidence for investigation.
