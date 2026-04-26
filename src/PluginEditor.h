#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class Timbro;

// Vintage analog look and feel inspired by classic 60s/70s studio gear
class VintageLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;
};

// Zone indicator strip showing active tonal zone
class ZoneStrip : public juce::Component
{
public:
    ZoneStrip();
    void paint(juce::Graphics&) override;
    void setDialValue(float value);

private:
    float dialValue = 5.0f;
};

class TimbroEditor : public juce::AudioProcessorEditor
{
public:
    explicit TimbroEditor(Timbro&);
    ~TimbroEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    Timbro& processor;
    VintageLookAndFeel vintageLnf;

    // Main dial
    juce::Slider dialKnob;

    // Zone display
    ZoneStrip zoneStrip;
    juce::Label zoneLabel;

    // Secondary controls
    juce::Slider inputKnob;
    juce::Slider outputKnob;
    juce::Label inputLabel;
    juce::Label outputLabel;

    // Bypass
    juce::ToggleButton bypassButton{"BYPASS"};

    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dialAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    void updateZoneLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbroEditor)
};
