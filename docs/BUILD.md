# Building OPENGOD

## Standalone core

Use CMake and a C++20 compiler. This target deliberately does not require CEF or a Chromium checkout.

```powershell
cmake -S . -B build/core
cmake --build build/core --config Release
ctest --test-dir build/core -C Release --output-on-failure
```

Select a generator explicitly if CMake cannot find the desired compiler. Keep different compiler/generator combinations in different build directories.

## TypeScript shell

Use Node.js 24 and the package scripts in the root `package.json`. Run `npm ci`, `npm test` and `npm run typecheck` from the repository root. `npm run build` compiles TypeScript and packages static assets into `ui/dist/`; run it before the native build. The produced static files are packaged with the native executable. Viewing these files in a normal browser verifies presentation only and cannot exercise native browsing or IPC.

## Native Windows browser

Use Windows x64 and a Visual Studio C++ toolchain compatible with the selected CEF distribution, including the Windows SDK. `CEF_ROOT` is the extracted binary SDK directory containing its CMake configuration, headers, libraries and resources. Do not point it at an archive or a CEF sample executable directory.

```powershell
$cefSdk = & ./tools/fetch-cef.ps1
cmake -S . -B build/browser -G "Visual Studio 17 2022" -A x64 -DOPENGOD_BUILD_BROWSER=ON -DCEF_ROOT="$cefSdk"
cmake --build build/browser --config Release
ctest --test-dir build/browser -C Release --output-on-failure
```

The SDK, runtime DLLs, sandbox library, resource packs, locale files and generated shell assets must come from the same build inputs. Follow the repository acquisition utility if present; inspect its pinned version and checksum before changing the dependency. CEF is large and excluded from Git.

The browser build must retain sandbox support. Do not solve a linker, startup or resource failure by adding `--no-sandbox`, disabling web security or ignoring certificate errors. Investigate toolset compatibility, resource paths and subprocess initialization instead. A successful core build does not verify any of these native requirements.

See [TESTING.md](TESTING.md) for the native launch checklist. Distribution, signing, installer and automatic update instructions remain future release work.

## Pinned integration

The acquisition script selects CEF `152.0.6+g708dc14+chromium-152.0.7977.83`, Windows x64 minimal, with upstream archive SHA-1 `e5e3020627f4528bd43e22f4c4970000b0458e99`. The checksum detects archive mismatch; it is not a claim of independent supply-chain attestation. The local configuration used CMake 4.0.1, Node.js 24.20.0 and MSVC 19.44.35222 through the Visual Studio 2022 generator.

Since CEF M138, Windows sandbox integration uses the SDK `bootstrap.exe` loading an application DLL with exported `RunWinMain`. OPENGOD packages the bootstrap as `opengod.exe` beside `opengod.dll`; see the [official CEF sandbox setup](https://chromiumembedded.github.io/cef/sandbox_setup.html). Do not substitute an old monolithic executable sample.

After a successful Release build, run `build/browser/bin/Release/opengod.exe`. Keep its adjacent DLLs, resource packs, locales and `ui/` directory together. Configuration and native Release compilation have succeeded locally. Core CTest and six UI tests pass. Native launch through the sandbox bootstrap, new-tab rendering, real MDN rendering with synchronized title and basic create/switch/close interactions have been verified at 1360 × 900 on 2026-09-10. See TESTING.md for remaining gates and the known keyboard-focus issue.
The native DLL and CEF wrapper use a consistent static CRT. CEF shared-linker delay-load flags are filtered to the libraries actually imported by OPENGOD (`libcef` and `user32`), avoiding irrelevant delay-load warnings while preserving the sandbox bootstrap. Do not remove required delay-loading or change CRT linkage to suppress a build warning.

The native CMake adapter normalizes `CEF_ROOT` to forward slashes before invoking CEF macros. This accepts native PowerShell paths without CMake interpreting a Windows user-directory prefix as an escape sequence. On 2026-09-10, reconfiguration with the backslash-containing cached SDK path and the Release build in `build/browser` both passed. The resulting `build/browser/bin/Release/opengod.exe` launched and rendered the new-tab page. Core CTest passed in both `build/core` and `build/browser`; all eight current UI tests and typecheck passed. Real website rendering and shortcut routing have not been reverified for this rebuilt package.

## Current audio/frame build — 2026-09-12

Current UI suite: nine tests plus typecheck. Core CTest and native Release build pass. Audio uses Windows XAudio2 and CEF Alloy; see AUDIO.md for native output verification. The tested working build is also available at `build/bin/Release/opengod.exe`; the documented `build/browser` package has also been rebuilt successfully with the final sources. Imported platform DLL delay-load list now includes gdi32, ole32 and comctl32 alongside libcef and user32. Earlier keyboard-focus gaps were resolved for Ctrl+L/Ctrl+T and reload in the two-tab runtime check.

## Current identity — 2026-09-14

The current switch is OPENGOD_BUILD_BROWSER and the native package is opengod.exe plus opengod.dll. Icon resources are included in the DLL and applied to the copied bootstrap by Windows PowerShell during packaging. The source checkout directory is unchanged. Older verification entries describe historical builds; use the current executable names and commands above.
