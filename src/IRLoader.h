#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

// Loads and applies an IR cabinet convolution
class IRLoader
{
public:
    IRLoader();
    ~IRLoader();

    // Load IR from memory (binary resource). If prepare() has not been called
    // yet, the data is held until prepare() runs — loadImpulseResponse must
    // not be called before Convolution::prepare or it has no effect.
    void loadFromMemory(const void* data, size_t dataSize);

    // Load IR from a .wav file path
    void loadFromFile(const juce::String& filePath);

    // Prepare for playback. Must be called before audio starts; if a pending
    // IR is held, it is committed to the convolver here.
    void prepare(double sampleRate, int samplesPerBlock);

    // Process mono audio in-place
    void process(float* buffer, int numSamples);

    // Check if loaded
    bool isLoaded() const { return loaded; }

    // Reset
    void reset();

private:
    void commitPendingIR();

    juce::dsp::Convolution convolution;
    bool loaded = false;
    bool prepared = false;

    // Pending IR data (weak pointer — JUCE BinaryData is static, lives forever)
    const void* pendingData = nullptr;
    size_t pendingSize = 0;

    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IRLoader)
};
