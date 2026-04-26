#include <NAM/get_dsp.h>
#include <nlohmann/json.hpp>
#include "NAMEngine.h"

NAMEngine::NAMEngine() = default;
NAMEngine::~NAMEngine() = default;

bool NAMEngine::loadModel(const void* data, size_t dataSize)
{
    if (data == nullptr || dataSize == 0)
    {
        juce::Logger::writeToLog("NAMEngine::loadModel: empty data");
        return false;
    }

    try
    {
        const auto* begin = static_cast<const char*>(data);
        const auto* end   = begin + dataSize;
        auto config = nlohmann::json::parse(begin, end);

        auto newModel = nam::get_dsp(config);
        if (newModel == nullptr)
        {
            juce::Logger::writeToLog("NAMEngine::loadModel: get_dsp returned null");
            return false;
        }

        model = std::move(newModel);
        modelSampleRate = nam::get_sample_rate_from_nam_file(config);
        model->prewarm();
        return true;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog(juce::String("NAMEngine::loadModel failed: ") + e.what());
        return false;
    }
}

bool NAMEngine::loadModelFromFile(const juce::String& filePath)
{
    try
    {
        auto newModel = nam::get_dsp(std::filesystem::path(filePath.toStdString()));
        if (newModel == nullptr)
        {
            juce::Logger::writeToLog("NAMEngine::loadModelFromFile: get_dsp returned null");
            return false;
        }

        model = std::move(newModel);
        model->prewarm();
        return true;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog(juce::String("NAMEngine::loadModelFromFile failed: ") + e.what());
        return false;
    }
}

void NAMEngine::prepare(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    if (model)
        model->prewarm();
}

void NAMEngine::process(float* buffer, int numSamples)
{
    if (!isLoaded() || numSamples <= 0)
        return;

    float* inputChannels[1] = {buffer};
    float* outputChannels[1] = {buffer};
    model->process(inputChannels, outputChannels, numSamples);
}

bool NAMEngine::isLoaded() const
{
    return model != nullptr;
}

double NAMEngine::getModelSampleRate() const
{
    return modelSampleRate;
}

void NAMEngine::reset()
{
    model.reset();
    modelSampleRate = -1.0;
}
