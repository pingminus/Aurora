# Native per-tab audio

## API investigation and integration

The pinned CEF 152 headers expose `CefBrowserHost::SetAudioMuted` and `CefAudioHandler` PCM capture callbacks, but no per-browser volume setter. The inspected CEF implementation creates its browser loopback with `mute_source = true`; capture replaces normal local playback. The capture implementation is associated with the Alloy browser host, so integration must select the supported Alloy runtime while preserving Chromium sandbox setup.

OPENGOD implements volume through `src/audio/tab_audio_output.h` and `.cpp`, using Windows XAudio2. This is an OS playback path, not injected page JavaScript or a rewrite of website audio elements. The browser adapter owns one output per tab and forwards planar float packets from CEF. The adapter requests stereo, 48 kHz, 480-frame capture packets; the backend accepts mono/stereo at 8–192 kHz and packets up to 50 ms.

The adapter must prevent Chromium's original audio path from bypassing OPENGOD gain during capture setup or failure. Keep initial capture/mute handling consistent with the inspected CEF runtime; do not assume the public mute setter controls captured PCM. Effective native gain is zero when the tab is muted, otherwise its stored volume divided by 100. Muting must preserve the stored volume. The backend accepts linear gain 0–1 and rejects nonfinite or out-of-range values.

## Lifetime, buffering and failures

`start`, `push`, `stop`, `set_gain`, `ready` and `error` are thread-safe. Start/stop are asynchronous; stream-generation changes discard old pending PCM and cause the worker to destroy the old voice. A dedicated per-tab worker initializes COM once and owns the XAudio2 engine, mastering voice and source voice. It changes source-voice gain, including already queued data. The owner must stop further callbacks before destroying the output; destruction joins the worker.

Pending PCM is bounded to 100 ms and at most 12 packets, plus at most three packets submitted to the device. Each packet is at most 50 ms. Overflow drops the oldest pending packet to favor current audio; `dropped_packets()` exposes the lifetime count. Each packet copies/interleaves its input, clamps samples to the normalized range and replaces nonfinite samples with zero. Device buffers remain owned until completion or voice destruction. Source-voice destruction waits for callbacks before releasing PCM, as specified by [Microsoft's DestroyVoice contract](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2voice-destroyvoice).

COM, device creation, submission and gain failures produce explicit `error()` text and disable that stream. A subsequent `start` clears the previous error and retries initialization. No silent fallback to uncontrolled website audio is implemented. `ready()` reports device initialization; `completed_frames()` reports buffer completion, not audible speaker output. Both counters are cumulative for the output object's lifetime. Native linking requires `xaudio2` and `ole32`.

## Verification and limitations

An isolated MSVC Release smoke build passed `/W4 /WX`. The actual Windows default audio device accepted silent stereo PCM and completed 11,040 frames during a test covering invalid formats/gain, bounded overflow, 100 rapid stop/restart transitions and a final mono restart. The overflow test reported 9,987 dropped packets instead of unbounded queue growth. The temporary test artifacts are in the ignored `.cache/audio-smoke/` directory.

This evidence verifies device initialization, callbacks and lifecycle behavior; it does not prove audible per-tab gain ratios. Browser-level verification is recorded below. Video/media-element capture and renderer-crash recovery still require dedicated runtime tests.

The current backend uses the default Windows output device and one engine/worker per captured tab. It does not expose device selection, surround layouts, exclusive mode or a cross-platform audio backend. Device failures are reported and require a new stream start; automatic device-loss recovery is not claimed. Packets are played in capture arrival order; the CEF presentation timestamp is not currently used for advanced A/V clock correction. Queue bounds limit memory and accumulated latency, but no production latency baseline is claimed.

## Two-tab output verification (2026-09-11)

The packaged Alloy browser ran two simultaneously playing Web Audio test websites from `tools/audio-fixture.mjs`: A at 440 Hz and B at 660 Hz. `tools/audio-loopback` measured only the OpenGod process tree through WASAPI (no microphone or system-wide capture, no saved PCM). Each measurement contains 96,000 frames over two seconds with zero discontinuities and timestamp errors. These are measured browser output streams, not a claim of calibrated physical speaker loudness.

| State | A 440 Hz amplitude | B 660 Hz amplitude |
|---|---:|---:|
| Both 100%, stable | 0.169276 | 0.112897 |
| A 100%, B 50% | 0.169275 | 0.068027 |
| B reloaded at 50% and playback restarted | 0.169258 | 0.068020 |
| B muted, stored volume 50% | 0.169275 | 0.000000346 |
| A muted, B 100% | 0.000000275 | 0.114634 |
| A changed to 50% while muted | 0.000000414 | 0.114566 |
| A unmuted at stored 50%, B 100% | 0.099699 | 0.114605 |
| Playing A closed, B remains muted | 0.000000019 | 0.000000272 |

This establishes real output changes and isolation, including background A, saved volume while muted, reload restoration and closure without orphan output. The 50% control sets linear native gain 0.5; the capture measurement is not calibrated and must not be described as half perceived loudness. Detailed numeric records are in `audio-verification.json`.

Reproduce: run `node tools/audio-fixture.mjs`; open `http://127.0.0.1:8765/tone-a` and `/tone-b` in separate OpenGod tabs and press each Play button. Build `tools/audio-loopback` with CMake, then run its Release executable with `--pid <OpenGod root process ID> --seconds 2`. Change only one tab at a time and compare the two frequency amplitudes. The fixture JavaScript generates test content; production volume never injects page scripts.

The native lifecycle smoke test is now reproducible from `tests/audio`: configure into `build/audio-smoke`, build Release and run CTest. It requires a Windows output device and sends silent PCM only. Core tests cover tab defaults, invalid integers, mute/unmute, navigation, duplicate/reopen and closed IDs; UI protocol tests reject malformed audio snapshots. Full malformed-command runtime injection, forced renderer crash, HTML media/video and audio device-loss recovery remain verification gaps.
