#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

Timbro::Timbro()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    dialParam = apvts.getRawParameterValue("dial");
    inputGainParam = apvts.getRawParameterValue("inputGain");
    outputGainParam = apvts.getRawParameterValue("outputGain");
    bypassParam = apvts.getRawParameterValue("bypass");

    // Load NAM profiles from binary resources. NAMEngine::loadModel now
    // deserializes JSON directly from memory — safe inside the AU sandbox.
    zoneBlender.loadZoneProfile(0, BinaryData::clean_nam,  BinaryData::clean_namSize);
    zoneBlender.loadZoneProfile(1, BinaryData::warm_nam,   BinaryData::warm_namSize);
    zoneBlender.loadZoneProfile(2, BinaryData::crunch_nam, BinaryData::crunch_namSize);
    zoneBlender.loadZoneProfile(3, BinaryData::drive_nam,  BinaryData::drive_namSize);
    zoneBlender.loadZoneProfile(4, BinaryData::lead_nam,   BinaryData::lead_namSize);

    // Queue cabinet IRs. Data is held inside IRLoader and committed to the
    // juce::dsp::Convolution in IRLoader::prepare(), which the host triggers
    // via prepareToPlay → ZoneBlender::prepare.
    zoneBlender.loadZoneIR(0, BinaryData::clean_wav,  BinaryData::clean_wavSize);
    zoneBlender.loadZoneIR(1, BinaryData::warm_wav,   BinaryData::warm_wavSize);
    zoneBlender.loadZoneIR(2, BinaryData::crunch_wav, BinaryData::crunch_wavSize);
    zoneBlender.loadZoneIR(3, BinaryData::drive_wav,  BinaryData::drive_wavSize);
    zoneBlender.loadZoneIR(4, BinaryData::lead_wav,   BinaryData::lead_wavSize);

    juce::Logger::writeToLog("Timbro: ctor loaded " + juce::String(zoneBlender.getLoadedZoneCount()) + "/5 zones");
}

Timbro::~Timbro() = default;

juce::AudioProcessorValueTreeState::ParameterLayout Timbro::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"dial", 1}, "Dial",
        juce::NormalisableRange<float>(0.0f, 10.0f, 0.01f), 5.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"inputGain", 1}, "Input",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1}, "Output",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));

    return {params.begin(), params.end()};
}

void Timbro::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    zoneBlender.prepare(sampleRate, samplesPerBlock);

    // Noise gate - fixed internal settings
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumInputChannels());
    noiseGate.prepare(spec);
    noiseGate.setThreshold(-60.0f);
    noiseGate.setRatio(10.0f);
    noiseGate.setAttack(1.0f);
    noiseGate.setRelease(50.0f);
}

void Timbro::releaseResources()
{
    zoneBlender.releaseResources();
    noiseGate.reset();
}

bool Timbro::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainInput = layouts.getMainInputChannelSet();
    const auto& mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != mainOutput)
        return false;

    if (mainInput != juce::AudioChannelSet::mono()
        && mainInput != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void Timbro::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Bypass
    if (bypassParam->load() > 0.5f)
        return;

    const float inputGainDb = inputGainParam->load();
    const float outputGainDb = outputGainParam->load();
    const float dialValue = dialParam->load();

    // Input gain
    if (std::abs(inputGainDb) > 0.01f)
    {
        const float inputGainLinear = juce::Decibels::decibelsToGain(inputGainDb);
        buffer.applyGain(inputGainLinear);
    }

    // Noise gate (before NAM processing)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        noiseGate.process(context);
    }

    // Process through NAM zone blender
    zoneBlender.process(buffer, dialValue);

    // Output gain
    if (std::abs(outputGainDb) > 0.01f)
    {
        const float outputGainLinear = juce::Decibels::decibelsToGain(outputGainDb);
        buffer.applyGain(outputGainLinear);
    }

    // Update output levels for VU meter
    outputLevelLeft.store(buffer.getRMSLevel(0, 0, buffer.getNumSamples()));
    if (buffer.getNumChannels() > 1)
        outputLevelRight.store(buffer.getRMSLevel(1, 0, buffer.getNumSamples()));
    else
        outputLevelRight.store(outputLevelLeft.load());
}

juce::AudioProcessorEditor* Timbro::createEditor()
{
    return new TimbroEditor(*this);
}

void Timbro::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Timbro::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Timbro();
}
