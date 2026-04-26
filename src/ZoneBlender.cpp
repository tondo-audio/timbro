#include "ZoneBlender.h"

ZoneBlender::ZoneBlender() = default;
ZoneBlender::~ZoneBlender() = default;

void ZoneBlender::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    tempBufferA.setSize(2, samplesPerBlock);
    tempBufferB.setSize(2, samplesPerBlock);

    for (auto& engine : namEngines)
        engine.prepare(sampleRate, samplesPerBlock);

    // IRLoader::prepare() primes Convolution AND commits any pending IR that
    // was queued before prepare was called. Must run after NAM prepare.
    for (auto& ir : irLoaders)
        ir.prepare(sampleRate, samplesPerBlock);
}

void ZoneBlender::releaseResources()
{
    // Reset convolvers' internal state but DO NOT throw away the NAM models
    // or the IR pending-data. Logic calls releaseResources between transport
    // stops, and it would be wasteful (and buggy) to reload the models from
    // scratch each time prepareToPlay fires again.
    // We intentionally skip engine.reset() / ir.reset() here — prepare() is
    // idempotent for both.
}

void ZoneBlender::getZoneBlend(float dialValue, int& zoneA, int& zoneB, float& blendFactor)
{
    dialValue = juce::jlimit(0.0f, 10.0f, dialValue);

    zoneA = juce::jlimit(0, 4, static_cast<int>(dialValue / 2.0f));

    if (zoneA >= 4)
    {
        zoneA = 4;
        zoneB = 4;
        blendFactor = 0.0f;
        return;
    }

    float posInZone = (dialValue - static_cast<float>(zoneA) * 2.0f) / 2.0f;

    if (posInZone <= 0.5f)
    {
        zoneB = zoneA;
        blendFactor = 0.0f;
    }
    else
    {
        zoneB = juce::jlimit(0, 4, zoneA + 1);
        blendFactor = (posInZone - 0.5f) * 2.0f;
    }
}

const char* ZoneBlender::getZoneName(float dialValue)
{
    int zoneA, zoneB;
    float blend;
    getZoneBlend(dialValue, zoneA, zoneB, blend);
    return kZones[static_cast<size_t>(zoneA)].name;
}

void ZoneBlender::process(juce::AudioBuffer<float>& buffer, float dialValue)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0)
        return;

    int zoneA, zoneB;
    float blendFactor;
    getZoneBlend(dialValue, zoneA, zoneB, blendFactor);

    const auto idxA = static_cast<size_t>(zoneA);
    const auto idxB = static_cast<size_t>(zoneB);

    // Log the first time zoneA is not loaded. This makes silent load failures
    // visible without flooding Console every block.
    if (!namEngines[idxA].isLoaded() && !loggedMissingZone[idxA])
    {
        juce::Logger::writeToLog("ZoneBlender: zone " + juce::String(zoneA) + " NAM not loaded — passthrough");
        loggedMissingZone[idxA] = true;
    }

    // NAM is mono: process channel 0 only, then replicate to other channels.
    float* ch0 = buffer.getWritePointer(0);

    if (blendFactor < 0.001f)
    {
        // Pure zone A: NAM then IR, each gracefully skipped if not ready.
        if (namEngines[idxA].isLoaded())
            namEngines[idxA].process(ch0, numSamples);
        if (irLoaders[idxA].isLoaded())
            irLoaders[idxA].process(ch0, numSamples);
    }
    else
    {
        // Blend between zone A and zone B.
        tempBufferA.copyFrom(0, 0, buffer, 0, 0, numSamples);
        tempBufferB.copyFrom(0, 0, buffer, 0, 0, numSamples);

        if (namEngines[idxA].isLoaded())
            namEngines[idxA].process(tempBufferA.getWritePointer(0), numSamples);
        if (irLoaders[idxA].isLoaded())
            irLoaders[idxA].process(tempBufferA.getWritePointer(0), numSamples);

        if (namEngines[idxB].isLoaded())
            namEngines[idxB].process(tempBufferB.getWritePointer(0), numSamples);
        if (irLoaders[idxB].isLoaded())
            irLoaders[idxB].process(tempBufferB.getWritePointer(0), numSamples);

        const float* aData = tempBufferA.getReadPointer(0);
        const float* bData = tempBufferB.getReadPointer(0);
        const float invBlend = 1.0f - blendFactor;

        for (int i = 0; i < numSamples; ++i)
            ch0[i] = aData[i] * invBlend + bData[i] * blendFactor;
    }

    // Copy mono result to all other channels.
    for (int ch = 1; ch < numChannels; ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);
}

bool ZoneBlender::loadZoneProfile(int zoneIndex, const void* data, size_t dataSize)
{
    if (zoneIndex < 0 || zoneIndex >= kNumZones)
        return false;
    bool ok = namEngines[static_cast<size_t>(zoneIndex)].loadModel(data, dataSize);
    if (!ok)
        juce::Logger::writeToLog("ZoneBlender: failed to load NAM for zone " + juce::String(zoneIndex));
    return ok;
}

bool ZoneBlender::loadZoneProfileFromFile(int zoneIndex, const juce::String& filePath)
{
    if (zoneIndex < 0 || zoneIndex >= kNumZones)
        return false;
    return namEngines[static_cast<size_t>(zoneIndex)].loadModelFromFile(filePath);
}

void ZoneBlender::loadZoneIR(int zoneIndex, const void* data, size_t dataSize)
{
    if (zoneIndex >= 0 && zoneIndex < kNumZones)
        irLoaders[static_cast<size_t>(zoneIndex)].loadFromMemory(data, dataSize);
}

void ZoneBlender::loadZoneIRFromFile(int zoneIndex, const juce::String& filePath)
{
    if (zoneIndex >= 0 && zoneIndex < kNumZones)
        irLoaders[static_cast<size_t>(zoneIndex)].loadFromFile(filePath);
}

int ZoneBlender::getLoadedZoneCount() const
{
    int n = 0;
    for (const auto& e : namEngines)
        if (e.isLoaded()) ++n;
    return n;
}
