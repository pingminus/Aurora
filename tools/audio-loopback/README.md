# Process-scoped audio verifier

Build with the Windows SDK and MSVC:

```powershell
cmake -S tools/audio-loopback -B build/audio-loopback -G "Visual Studio 17 2022" -A x64
cmake --build build/audio-loopback --config Release
ctest --test-dir build/audio-loopback -C Release --output-on-failure
build/audio-loopback/Release/aurora-audio-loopback.exe --pid 1234 --seconds 2
```

Supply the actual Aurora browser/root PID. Capture uses `VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK` with `PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE`. There is no desktop-wide, exclusion-mode or microphone fallback. It requires the Windows process-loopback API (Windows build 20348 or newer). The implementation follows the activation and format contracts illustrated by [Microsoft's application loopback sample](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/ApplicationLoopback), without its file-recording or Media Foundation worker pipeline.

The tool requests 48 kHz stereo PCM16 with Windows conversion enabled. It retains at most the requested 0.25–10 seconds of PCM in memory and writes only JSON metrics to stdout. It creates no audio recording. Activation times out after ten seconds; capture allows one extra second for startup before reporting partial data. A missing target, activation/device failure or zero received packets returns an error and exit code 1. Missing packets are never fabricated as silence.

The result distinguishes `captured` from `partial`, includes actual frame count/duration, silent packets, discontinuities, timestamp errors, RMS and peak, and estimates the 440 Hz and 660 Hz peak sinusoid amplitudes with a Hann-weighted Fourier projection. Stereo estimates are calculated separately; the combined amplitude is their quadratic mean, so opposite-phase channels do not cancel. Amplitudes are normalized digital samples, not measured sound-pressure levels or proof that physical speakers are audible.

For gain verification, play distinct 440/660 Hz tones in two Aurora tabs, capture a baseline, change one tab's gain, and capture again after settling. Compare that tone's amplitude ratio while checking the other tone remains stable. Treat partial captures or discontinuities as reduced-confidence measurements and repeat under steady conditions. Tone clipping, unrelated sound within Aurora, OS processing and frequency drift can affect estimates. The first discontinuity flag can be a stream-start marker; the tool reports it rather than hiding it.

The `/W4 /WX` Release build and synthetic CTest passed. The self-test uses opposite-phase stereo tones, known 0.25/0.10 amplitudes, a quarter-gain comparison and silence. It performs no capture; actual process-loopback functionality must be verified separately with an explicitly supplied Aurora PID.
