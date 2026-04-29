#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ZoneBlender.h"

class Timbro : public juce::AudioProcessor
{
public:
    Timbro();
    ~Timbro() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Mono RMS levels for the editor's small in/out meters (thread-safe).
    // Input is sampled after the input gain (so the meter shows what's
    // actually being fed into NAM); output is sampled at the very end of
    // processBlock, after output gain.
    float getInputLevel() const { return inputLevel.load(); }
    float getOutputLevel() const { return outputLevel.load(); }

private:
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    ZoneBlender zoneBlender;

    // Noise gate
    juce::dsp::NoiseGate<float> noiseGate;

    // Parameter pointers (cached)
    std::atomic<float>* dialParam = nullptr;
    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* gateThresholdParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    // Last threshold pushed into the gate, so we only call setThreshold()
    // when the user actually moves the knob.
    float lastGateThresholdDb = 1.0f;

    // Mono RMS sampled per processBlock; the editor smooths/decays for display.
    std::atomic<float> inputLevel{0.0f};
    std::atomic<float> outputLevel{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Timbro)
};
