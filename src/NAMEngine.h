#pragma once

#include <memory>
#include <string>
#include <vector>
#include <juce_core/juce_core.h>

// Forward declare NAM DSP
namespace nam
{
class DSP;
}

// Represents a single NAM model that can process audio
class NAMEngine
{
public:
    NAMEngine();
    ~NAMEngine();

    // Load a .nam model from memory (binary resource)
    bool loadModel(const void* data, size_t dataSize);

    // Load a .nam model from file path
    bool loadModelFromFile(const juce::String& filePath);

    // Prepare for playback
    void prepare(double sampleRate, int samplesPerBlock);

    // Process a mono buffer in-place (float)
    void process(float* buffer, int numSamples);

    // Check if a model is loaded
    bool isLoaded() const;

    // Sample rate the model was trained at (-1 if unknown)
    double getModelSampleRate() const;

    // Release resources
    void reset();

private:
    std::unique_ptr<nam::DSP> model;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    double modelSampleRate = -1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NAMEngine)
};
