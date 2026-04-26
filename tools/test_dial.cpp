// Standalone test: drives ZoneBlender directly with a sine input, sweeps
// the dial across all five zones at 48 kHz, and asserts that the output
// RMS changes meaningfully between zones — proving the NAM stage is alive.

#include "BinaryData.h"
#include "ZoneBlender.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 512;
constexpr int    kNumWarmup  = 16; // blocks to settle internal state
constexpr int    kNumMeas    = 64; // blocks measured per zone

float measureZone(ZoneBlender& zb, float dial)
{
    juce::AudioBuffer<float> buffer(1, kBlockSize);

    auto fillSine = [&](int blockIdx) {
        const double freq = 220.0;
        const double twoPi = 2.0 * juce::MathConstants<double>::pi;
        for (int i = 0; i < kBlockSize; ++i)
        {
            const double t = (double) (blockIdx * kBlockSize + i) / kSampleRate;
            buffer.setSample(0, i, 0.3f * (float) std::sin(twoPi * freq * t));
        }
    };

    // Warmup
    for (int b = 0; b < kNumWarmup; ++b)
    {
        fillSine(b);
        zb.process(buffer, dial);
    }

    // Measure mean RMS over kNumMeas blocks
    double accum = 0.0;
    for (int b = 0; b < kNumMeas; ++b)
    {
        fillSine(kNumWarmup + b);
        zb.process(buffer, dial);
        accum += buffer.getRMSLevel(0, 0, kBlockSize);
    }
    return (float) (accum / kNumMeas);
}
} // namespace

int main()
{
    ZoneBlender zb;

    bool allLoaded = true;
    allLoaded &= zb.loadZoneProfile(0, BinaryData::clean_nam,  BinaryData::clean_namSize);
    allLoaded &= zb.loadZoneProfile(1, BinaryData::warm_nam,   BinaryData::warm_namSize);
    allLoaded &= zb.loadZoneProfile(2, BinaryData::crunch_nam, BinaryData::crunch_namSize);
    allLoaded &= zb.loadZoneProfile(3, BinaryData::drive_nam,  BinaryData::drive_namSize);
    allLoaded &= zb.loadZoneProfile(4, BinaryData::lead_nam,   BinaryData::lead_namSize);

    zb.loadZoneIR(0, BinaryData::clean_wav,  BinaryData::clean_wavSize);
    zb.loadZoneIR(1, BinaryData::warm_wav,   BinaryData::warm_wavSize);
    zb.loadZoneIR(2, BinaryData::crunch_wav, BinaryData::crunch_wavSize);
    zb.loadZoneIR(3, BinaryData::drive_wav,  BinaryData::drive_wavSize);
    zb.loadZoneIR(4, BinaryData::lead_wav,   BinaryData::lead_wavSize);

    if (!allLoaded || zb.getLoadedZoneCount() != 5)
    {
        std::fprintf(stderr, "FAIL: only %d/5 zones loaded\n", zb.getLoadedZoneCount());
        return 2;
    }

    zb.prepare(kSampleRate, kBlockSize);

    struct ZonePoint { const char* name; float dial; };
    const ZonePoint points[] = {
        {"CLEAN",  0.0f},
        {"WARM",   3.0f},
        {"CRUNCH", 5.0f},
        {"DRIVE",  7.0f},
        {"LEAD",  10.0f},
    };

    std::vector<float> rms;
    std::printf("Input: 220 Hz sine, 0.3 amplitude, %d Hz, %d-sample blocks\n",
                (int) kSampleRate, kBlockSize);
    std::printf("%-8s %-6s %s\n", "Zone", "Dial", "RMS");
    std::printf("------------------------\n");
    for (const auto& p : points)
    {
        const float r = measureZone(zb, p.dial);
        rms.push_back(r);
        std::printf("%-8s %-6.1f %.6f\n", p.name, p.dial, r);
    }

    // Pass criterion: at least 3 distinct RMS clusters (within 1% tolerance).
    // A broken plugin would either produce identical RMS for every zone, or
    // identical RMS == input RMS (~ 0.212 for a 0.3-amplitude sine).
    int distinctZones = 0;
    for (size_t i = 0; i < rms.size(); ++i)
    {
        bool unique = true;
        for (size_t j = 0; j < i; ++j)
            if (std::abs(rms[i] - rms[j]) / std::max(rms[j], 1e-6f) < 0.01f)
                unique = false;
        if (unique) ++distinctZones;
    }
    std::printf("------------------------\n");
    std::printf("Distinct RMS levels across zones: %d / %zu\n", distinctZones, rms.size());

    if (distinctZones < 3)
    {
        std::fprintf(stderr, "FAIL: dial not changing tone meaningfully\n");
        return 1;
    }

    std::printf("PASS: dial sweep produces distinct tonal responses\n");
    return 0;
}
