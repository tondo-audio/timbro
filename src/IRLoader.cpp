#include "IRLoader.h"

IRLoader::IRLoader()
    : convolution(juce::dsp::Convolution::NonUniform{128})
{
}

IRLoader::~IRLoader() = default;

void IRLoader::loadFromMemory(const void* data, size_t dataSize)
{
    if (data == nullptr || dataSize == 0)
    {
        juce::Logger::writeToLog("IRLoader::loadFromMemory: empty data");
        return;
    }

    pendingData = data;
    pendingSize = dataSize;

    if (prepared)
        commitPendingIR();
}

void IRLoader::loadFromFile(const juce::String& filePath)
{
    juce::File irFile(filePath);
    if (!irFile.existsAsFile())
    {
        juce::Logger::writeToLog("IRLoader::loadFromFile: file not found: " + filePath);
        return;
    }

    if (!prepared)
    {
        juce::Logger::writeToLog("IRLoader::loadFromFile: not prepared yet — file path cannot be deferred");
        return;
    }

    convolution.loadImpulseResponse(irFile,
                                    juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::yes,
                                    0);
    loaded = true;
    pendingData = nullptr;
    pendingSize = 0;
}

void IRLoader::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;
    convolution.prepare(spec);
    prepared = true;

    if (pendingData != nullptr)
        commitPendingIR();
}

void IRLoader::commitPendingIR()
{
    convolution.loadImpulseResponse(pendingData, pendingSize,
                                    juce::dsp::Convolution::Stereo::no,
                                    juce::dsp::Convolution::Trim::yes,
                                    0); // 0 = use full IR length
    loaded = true;
}

void IRLoader::process(float* buffer, int numSamples)
{
    if (!loaded || !prepared)
        return;

    juce::dsp::AudioBlock<float> block(&buffer, 1, static_cast<size_t>(numSamples));
    juce::dsp::ProcessContextReplacing<float> context(block);
    convolution.process(context);
}

void IRLoader::reset()
{
    convolution.reset();
    loaded = false;
    prepared = false;
    pendingData = nullptr;
    pendingSize = 0;
}
