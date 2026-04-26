# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Timbro is a macOS Audio Unit (AU) plugin built with JUCE 8 and NeuralAmpModelerCore. It provides neural amp modeling through a single-knob interface: turn left for clean, turn right for high gain. Five tonal zones (CLEAN, WARM, CRUNCH, DRIVE, LEAD) blend smoothly via parallel NAM processing.

## Build Commands

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build (AU plugin auto-copies to ~/Library/Audio/Plug-Ins/Components/)
cmake --build build --config Release

# Allow unsigned AUs in Logic Pro
defaults write com.apple.Logic10 DoNotValidateAudioUnits -bool YES
```

Requirements: macOS 12+, CMake 3.22+, C++17 compiler.

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
- Plugin formats: AU and Standalone only (no VST3)
- No MIDI, no web browser, no CURL (all disabled via compile defs)
- Unsigned plugin — requires Logic Pro DoNotValidateAudioUnits flag
- All profiles are bundled (no runtime file loading for end users)
