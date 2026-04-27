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

    // Loudness compensation gain (linear) applied after model->process to
    // normalize all profiles to a common target loudness. 1.0 if the model
    // has no loudness metadata (legacy .nam files).
    float getLoudnessGain() const { return loudnessGain; }

    // Release resources
    void reset();

    // Target loudness in dB used for compensation. -12 dB sits 6 dB above
    // the NAM training pipeline's reamping reference (-18 dBFS) so the
    // plugin lands near commercial mix levels without needing a heavy
    // makeup pass on the output. With the ±15 dB clamp in
    // recomputeLoudnessGain this is still a safe operating point for any
    // reasonably trained model.
    static constexpr double kTargetLoudnessDb = -12.0;

private:
    void recomputeLoudnessGain();

    // Measures the model's loudness empirically by processing a sine at
    // -18 dBFS RMS and reading back the output RMS. Used as a fallback for
    // legacy .nam files that don't carry loudness metadata, so any third-
    // party profile a user drops in normalizes to the same target.
    double measureLoudnessDb();

    std::unique_ptr<nam::DSP> model;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    double modelSampleRate = -1.0;
    float loudnessGain = 1.0f;
    bool loudnessGainComputed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NAMEngine)
};
