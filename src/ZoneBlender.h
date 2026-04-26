#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "NAMEngine.h"
#include "IRLoader.h"

// The 5 tonal zones
enum class Zone
{
    Clean = 0,  // 0-2
    Warm,       // 2-4
    Crunch,     // 4-6
    Drive,      // 6-8
    Lead,       // 8-10
    NumZones
};

struct ZoneInfo
{
    const char* name;
    float rangeStart;
    float rangeEnd;
};

constexpr std::array<ZoneInfo, 5> kZones = {{
    {"CLEAN",  0.0f, 2.0f},
    {"WARM",   2.0f, 4.0f},
    {"CRUNCH", 4.0f, 6.0f},
    {"DRIVE",  6.0f, 8.0f},
    {"LEAD",   8.0f, 10.0f}
}};

class ZoneBlender
{
public:
    ZoneBlender();
    ~ZoneBlender();

    void prepare(double sampleRate, int samplesPerBlock);
    void releaseResources();

    // Process audio buffer with the given dial position (0-10)
    void process(juce::AudioBuffer<float>& buffer, float dialValue);

    // Get zone name for a given dial position
    static const char* getZoneName(float dialValue);

    // Get zone index and blend factor
    static void getZoneBlend(float dialValue, int& zoneA, int& zoneB, float& blendFactor);

    // Load NAM profiles and IR cabinets
    bool loadZoneProfile(int zoneIndex, const void* data, size_t dataSize);
    bool loadZoneProfileFromFile(int zoneIndex, const juce::String& filePath);
    void loadZoneIR(int zoneIndex, const void* data, size_t dataSize);
    void loadZoneIRFromFile(int zoneIndex, const juce::String& filePath);

    // Number of zones whose NAM profile successfully loaded
    int getLoadedZoneCount() const;

private:
    static constexpr int kNumZones = 5;

    std::array<NAMEngine, kNumZones> namEngines;
    std::array<IRLoader, kNumZones> irLoaders;

    // Temp buffers for parallel processing
    juce::AudioBuffer<float> tempBufferA;
    juce::AudioBuffer<float> tempBufferB;

    // Per-zone first-time "not loaded" log latch
    std::array<bool, kNumZones> loggedMissingZone{};

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoneBlender)
};
