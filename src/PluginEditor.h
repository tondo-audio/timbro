#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

class Timbro;

// =============================================================================
// Flat, Surge XT-inspired LookAndFeel — drawn entirely with juce::Graphics.
// VOICE is rendered slightly differently (zone tick marks on the ring) when
// the slider's componentID is "voice".
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
};


// Slider subclass that triggers a repaint on mouse enter/exit so the
// LookAndFeel can render a hover state via juce::Slider::isMouseOver().
class HoverableSlider : public juce::Slider
{
public:
    using juce::Slider::Slider;
    void mouseEnter(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseEnter(e);
        repaint();
    }
    void mouseExit(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseExit(e);
        repaint();
    }
};


// Tiny horizontal level meter — peak-hold display fed by an atomic
// linear-RMS value sampled by the audio thread. Decays smoothly so the
// bar isn't strobing at the buffer rate.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter(std::function<float()> levelFn);
    ~LevelMeter() override;

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    std::function<float()> getLevel;
    float displayLevel = 0.0f;

    JUCE_DECLARE_NON_COPYABLE(LevelMeter)
};


// 5 flat dots above the VOICE knob plus a dynamic zone-name label.
// Dot brightness interpolates between two adjacent zones during a blend;
// the label tracks the dominant zone.
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

    // VOICE is the centrepiece (big knob). INPUT / GATE / OUTPUT are the
    // per-instrument trim row underneath. BYPASS sits in the title bar.
    juce::ToggleButton bypassButton{"BYPASS"};
    HoverableSlider inputKnob;
    HoverableSlider gateKnob;
    HoverableSlider outputKnob;
    HoverableSlider voiceKnob;

    // Numeric readouts under each trim knob. VOICE uses the ZoneIndicator
    // as its readout instead, so no separate label is needed there.
    juce::Label inputValueLabel;
    juce::Label gateValueLabel;
    juce::Label outputValueLabel;

    LevelMeter inputMeter;
    LevelMeter outputMeter;

    ZoneIndicator zoneIndicator;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  inputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  gateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  outputAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  voiceAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimbroEditor)
};
