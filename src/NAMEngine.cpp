#include <NAM/get_dsp.h>
#include <nlohmann/json.hpp>
#include <juce_audio_basics/juce_audio_basics.h>
#include "NAMEngine.h"
#include <cmath>

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
        recomputeLoudnessGain();
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
        recomputeLoudnessGain();
        return true;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog(juce::String("NAMEngine::loadModelFromFile failed: ") + e.what());
        return false;
    }
}

void NAMEngine::recomputeLoudnessGain()
{
    if (model == nullptr)
    {
        loudnessGain = 1.0f;
        return;
    }

    double measuredDb;
    const char* source;
    if (model->HasLoudness())
    {
        measuredDb = model->GetLoudness();
        source = "metadata";
    }
    else
    {
        measuredDb = measureLoudnessDb();
        source = "auto-measured";
    }

    // Clamp makeup so a wildly off measurement (e.g. legacy profile + a
    // calibration signal that barely engages it) can't blow the levels up
    // or smother them. ±15 dB covers well-trained pro models comfortably.
    const double rawMakeupDb = kTargetLoudnessDb - measuredDb;
    const double makeupDb = juce::jlimit(-15.0, 15.0, rawMakeupDb);
    loudnessGain = static_cast<float>(std::pow(10.0, makeupDb / 20.0));
    juce::Logger::writeToLog(juce::String("NAMEngine: loudness ") + source + " "
                             + juce::String(measuredDb, 2) + " dB → makeup "
                             + juce::String(makeupDb, 2) + " dB"
                             + (rawMakeupDb != makeupDb
                                ? " (clamped from " + juce::String(rawMakeupDb, 2) + ")"
                                : ""));
}

double NAMEngine::measureLoudnessDb()
{
    if (model == nullptr)
        return kTargetLoudnessDb;

    // Calibration signal: deterministic pink noise at -18 dBFS RMS.
    // Pink noise approximates the spectral shape of a real guitar signal
    // far better than a single sine, so it engages a clean amp's tone
    // shaping AND a high-gain amp's saturation. Result: cross-architecture
    // consistency.
    constexpr double sampleRate = 48000.0;
    constexpr int    blockSize  = 512;
    constexpr int    numBlocks  = 48;          // ~0.5 seconds total
    constexpr int    warmBlocks = 24;          // first half discarded
    constexpr double refRmsDb   = -18.0;

    const float targetRms = static_cast<float>(std::pow(10.0, refRmsDb / 20.0));

    // Tell the model the buffer size we'll use; some architectures allocate
    // internal scratch lazily and crash if process() outruns Reset.
    try
    {
        model->Reset(sampleRate, blockSize);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog(juce::String("NAMEngine::measureLoudnessDb: Reset threw: ") + e.what());
        return kTargetLoudnessDb;
    }

    // Pre-generate the full pink-noise buffer once (deterministic seed) and
    // pre-scale it to the target RMS, so per-block processing only loops.
    const int totalSamples = numBlocks * blockSize;
    std::vector<float> signal(totalSamples);
    {
        // Voss-McCartney pink noise: sum of N octave-spaced random sources,
        // each updated at a different rate. N=7 gives ~5 octaves of pink.
        juce::Random rng(0xCAFEBABE);
        constexpr int N = 7;
        std::array<float, N> rows{};
        for (auto& r : rows) r = rng.nextFloat() * 2.0f - 1.0f;
        unsigned counter = 0;
        double sumSqIn = 0.0;
        for (int i = 0; i < totalSamples; ++i)
        {
            ++counter;
            for (int k = 0; k < N; ++k)
            {
                if ((counter & ((1u << k) - 1u)) == 0)
                    rows[k] = rng.nextFloat() * 2.0f - 1.0f;
            }
            float s = 0.0f;
            for (auto v : rows) s += v;
            s /= (float) N;
            signal[i] = s;
            sumSqIn += (double) s * s;
        }
        const float currentRms = (float) std::sqrt(sumSqIn / (double) totalSamples);
        if (currentRms > 1e-9f)
        {
            const float scale = targetRms / currentRms;
            for (auto& s : signal) s *= scale;
        }
    }

    std::vector<float> in(blockSize);
    std::vector<float> out(blockSize, 0.0f);
    float* inPtr[1]  = { in.data() };
    float* outPtr[1] = { out.data() };

    double sumSq = 0.0;
    long   sampleCount = 0;

    for (int b = 0; b < numBlocks; ++b)
    {
        std::copy(signal.begin() + (b * blockSize),
                  signal.begin() + ((b + 1) * blockSize),
                  in.begin());

        try
        {
            model->process(inPtr, outPtr, blockSize);
        }
        catch (const std::exception& e)
        {
            juce::Logger::writeToLog(juce::String("NAMEngine::measureLoudnessDb: process threw: ") + e.what());
            return kTargetLoudnessDb;
        }

        if (b >= warmBlocks)
        {
            for (int i = 0; i < blockSize; ++i)
                sumSq += static_cast<double>(out[i]) * out[i];
            sampleCount += blockSize;
        }
    }

    // Restore the model to a clean state for real-time playback.
    try
    {
        model->Reset(sampleRate, blockSize);
        model->prewarm();
    }
    catch (...) {}

    if (sampleCount == 0)
        return kTargetLoudnessDb;

    const double rms = std::sqrt(sumSq / static_cast<double>(sampleCount));
    return 20.0 * std::log10(std::max(rms, 1e-9));
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

    if (loudnessGain != 1.0f)
        juce::FloatVectorOperations::multiply(buffer, loudnessGain, numSamples);
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
    loudnessGain = 1.0f;
}
