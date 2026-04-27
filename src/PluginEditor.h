#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class Timbro;

// =============================================================================
// Asset-based LookAndFeel — every visible control is rendered by blitting a
// frame of a pre-rendered Blender filmstrip on top of the panel image.
// =============================================================================
class TimbroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TimbroLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

private:
    juce::Image knobStrip;     // 128 vertical frames, square
    juce::Image switchStrip;   // 2 vertical frames (UP/ON, DOWN/OFF)
};


// Strip of 5 vintage amber lamps above the VOICE knob. Each lamp's brightness
// reflects how much of the corresponding zone is currently mixed in — at the
// midpoint of a transition both adjacent lamps glow at half intensity.
class ZoneIndicator : public juce::Component, private juce::Timer
{
public:
    explicit ZoneIndicator(juce::AudioProcessorValueTreeState& apvts);
    ~ZoneIndicator() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    juce::AudioProcessorValueTreeState& apvts;
    float lastDialValue = -1.0f;

    JUCE_DECLARE_NON_COPYABLE(ZoneIndicator)
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
    TimbroLookAndFeel lnf;

    juce::Image panelImage;

    // Four controls in a row: BYPASS toggle, INPUT knob, OUTPUT knob, VOICE knob
    juce::ToggleButton bypassButton{"BYPASS"};
    juce::Slider inputKnob;
    juce::Slider outputKnob;
    juce::Slider voiceKnob;

    ZoneIndicator zoneIndicator;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  voiceAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbroEditor)
};
