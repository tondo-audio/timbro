# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Timbro is a tri-platform audio plugin (macOS, Windows, Linux) built with JUCE 8 and NeuralAmpModelerCore. It provides neural amp modeling through a single-knob interface: turn left for clean, turn right for high gain. Five tonal zones (CLEAN, WARM, CRUNCH, DRIVE, LEAD) blend smoothly via parallel NAM processing.

## Build Commands

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build (auto-copies plugin to the user-level plugin folder for the
# current platform: ~/Library/Audio/Plug-Ins/{Components,VST3} on macOS,
# %COMMONPROGRAMFILES%\VST3 on Windows, ~/.vst3 on Linux)
cmake --build build --config Release

# macOS only — allow unsigned AUs in Logic Pro / GarageBand
defaults write com.apple.Logic10 DoNotValidateAudioUnits -bool YES
```

Requirements: CMake 3.22+, C++20 compiler, and one of:
- macOS 12+
- Windows 10+ (x64) with MSVC
- Linux x86_64 with the JUCE dev packages (libasound2-dev, libjack-jackd2-dev, libcurl4-openssl-dev, libfreetype-dev, libx11-dev, libxcomposite-dev, libxcursor-dev, libxext-dev, libxinerama-dev, libxrandr-dev, libxrender-dev, libwebkit2gtk-4.1-dev, libglu1-mesa-dev, mesa-common-dev)

## CI / Release

Single workflow: `.github/workflows/release.yml`. Triggers only on `v*` tag pushes — there is no continuous build on `main`. The workflow runs three matrix jobs (macos-14, windows-latest, ubuntu-latest), packages each into a zip (`ditto` on macOS, `Compress-Archive` on Windows, `zip` on Linux), and a final `release` job aggregates the artifacts into a single GitHub Release.

If a tag fails CI: delete the tag locally and on origin (`git tag -d vX && git push --delete origin vX`), fix the offending commit, re-tag.

## Architecture

**Signal chain:** Input → Noise Gate → [NAM zone A + NAM zone B in parallel with blend] → IR Convolution → Volume Output

Key classes:
- **`Timbro`** (`PluginProcessor`): Main AudioProcessor. Owns ZoneBlender, noise gate, and APVTS parameters (dial 0-10, input/output gain, bypass). Exposes atomic output levels for VU meter.
- **`ZoneBlender`**: Core audio engine. Holds 5 NAMEngine + 5 IRLoader instances. Given a dial value (0-10), determines which two adjacent zones to run in parallel and crossfades their outputs. Uses `getZoneBlend()` to compute zone indices and blend factor.
- **`NAMEngine`**: Wraps `nam::DSP` from NeuralAmpModelerCore. Loads `.nam` models from binary resources or files, processes mono audio.
- **`IRLoader`**: Wraps JUCE `dsp::Convolution` for cabinet impulse response. Loads `.wav` IRs from binary resources or files.
- **`TimbroEditor`** (`PluginEditor`): UI with analog 70s aesthetic via `AnalogLookAndFeel`. Main rotary dial, zone label, input/output knobs, bypass toggle, decorative `VUMeter`.

**Zone mapping** (defined in `ZoneBlender.h`): dial 0-2 = CLEAN, 2-4 = WARM, 4-6 = CRUNCH, 6-8 = DRIVE, 8-10 = LEAD. Blending between adjacent zones is continuous.

## Dependencies

Managed via CPM (cmake/CPM.cmake):
- JUCE 8.0.6
- NeuralAmpModelerCore (main branch)
- Eigen (header-only, for NAM)
- nlohmann_json (for NAM)

## Resources

NAM profiles (`.nam`) go in `resources/profiles/`, cabinet IRs (`.wav`) in `resources/cabinets/`. Files are named by zone (clean, warm, crunch, drive, lead). They are embedded as binary data via `juce_add_binary_data` at build time.

## Key Design Decisions

- NAM_SAMPLE_FLOAT=1 is defined — NAM processes float (not double)
- Plugin formats are platform-conditional via `TIMBRO_FORMATS`: AU+VST3+Standalone on macOS, VST3+Standalone on Windows and Linux
- C++20 is required (NAM's `slimmable_wavenet` uses `std::atomic<std::shared_ptr<T>>`, a C++20 addition that MSVC's STL strictly enforces)
- No MIDI, no web browser, JUCE_USE_CURL=0 set — but on Linux libcurl is still linked because juce_core.cpp emits residual curl references the macro doesn't gate; CURL::libcurl is added to both Timbro and test_dial under `if(UNIX AND NOT APPLE)`
- NAM's auxiliary tools (benchmodel, render) are kept out of the build via `EXCLUDE_FROM_ALL YES` on its CPMAddPackage — they hard-code the GCC-only `-Wno-error` flag, which MSVC rejects
- NAM's static-init architecture registrators are preserved via per-platform force-load: `-force_load` (Apple ld), `--whole-archive` (GNU ld), `/WHOLEARCHIVE:` (MSVC) — see the `if(APPLE)/elseif(MSVC)/elseif(UNIX)` block in CMakeLists.txt
- Unsigned plugin — macOS hosts that strictly validate (Logic AU) need `DoNotValidateAudioUnits`; Windows SmartScreen may prompt on first standalone launch
- All profiles are bundled (no runtime file loading for end users)
