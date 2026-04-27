#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ZoneBlender.h"
#include "BinaryData.h"

#include <array>
#include <cmath>

namespace
{
    // Layout — must match the asset pipeline (compose_panel.py / preview_mockup.py)
    constexpr int kWindowW       = 720;
    constexpr int kWindowH       = 280;
    constexpr int kCtrlDiameter  = 156;
    constexpr int kCtrlRowCY     = 148;
    constexpr int kBypassCX      = 105;
    constexpr int kInputCX       = 275;
    constexpr int kOutputCX      = 445;
    constexpr int kVoiceCX       = 615;

    // Zone indicator strip — sits in the dead space above the VOICE knob, fully
    // clear of both the title baseline and the visible knob graphic.
    constexpr int kZoneStripW    = 92;
    constexpr int kZoneStripH    = 18;
    constexpr int kZoneStripY    = 72;

    constexpr int kKnobFrameCount = 128;
    constexpr int kSwitchFrameCount = 2;

    juce::Rectangle<int> ctrlBounds(int cx)
    {
        return { cx - kCtrlDiameter / 2,
                 kCtrlRowCY - kCtrlDiameter / 2,
                 kCtrlDiameter, kCtrlDiameter };
    }

    void drawLamp(juce::Graphics& g, float cx, float cy, float radius, float brightness)
    {
        using juce::Colour;
        using juce::ColourGradient;

        if (brightness > 0.02f)
        {
            const float haloR = radius * (1.6f + 1.8f * brightness);
            ColourGradient halo(Colour::fromFloatRGBA(1.0f, 0.45f, 0.15f, 0.55f * brightness),
                                cx, cy,
                                Colour::fromFloatRGBA(1.0f, 0.45f, 0.15f, 0.0f),
                                cx + haloR, cy, true);
            g.setGradientFill(halo);
            g.fillEllipse(cx - haloR, cy - haloR, haloR * 2.0f, haloR * 2.0f);
        }

        // Dark socket bezel
        g.setColour(Colour(0xff0a0604));
        g.fillEllipse(cx - radius - 1.4f, cy - radius - 1.4f,
                      (radius + 1.4f) * 2.0f, (radius + 1.4f) * 2.0f);

        // Dome — interpolate from dim base to a warm amber-red
        const Colour off  = Colour(0xff331410);
        const Colour on   = Colour::fromFloatRGBA(1.0f, 0.55f, 0.22f, 1.0f);
        const Colour body = off.interpolatedWith(on, brightness);

        ColourGradient dome(body.brighter(0.4f * brightness),
                            cx - radius * 0.35f, cy - radius * 0.35f,
                            body.darker(0.6f),
                            cx + radius, cy + radius, true);
        g.setGradientFill(dome);
        g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

        // Specular pinpoint
        g.setColour(Colour::fromFloatRGBA(1.0f, 1.0f, 0.92f, 0.20f + 0.40f * brightness));
        const float specR = radius * 0.32f;
        g.fillEllipse(cx - radius * 0.42f - specR,
                      cy - radius * 0.42f - specR,
                      specR * 2.0f, specR * 2.0f);
    }
}

// =============================================================================
// TimbroLookAndFeel
// =============================================================================

TimbroLookAndFeel::TimbroLookAndFeel()
{
    knobStrip   = juce::ImageCache::getFromMemory(BinaryData::knob_filmstrip_png,
                                                    BinaryData::knob_filmstrip_pngSize);
    switchStrip = juce::ImageCache::getFromMemory(BinaryData::switch_strip_png,
                                                    BinaryData::switch_strip_pngSize);

    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFEDE0C8));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::tickColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::transparentBlack);
}

void TimbroLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float, float, juce::Slider&)
{
    if (! knobStrip.isValid())
        return;

    const int frameSize = knobStrip.getWidth();
    const int frame = juce::jlimit(0, kKnobFrameCount - 1,
                                    (int) std::round(sliderPos * (kKnobFrameCount - 1)));

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(knobStrip,
                x, y, width, height,
                0, frame * frameSize, frameSize, frameSize);
}

void TimbroLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                          bool /*shouldDrawAsHighlighted*/,
                                          bool /*shouldDrawAsDown*/)
{
    if (! switchStrip.isValid())
        return;

    const int frameSize = switchStrip.getWidth();
    // toggleState == true  → bypass engaged → DOWN frame (LED off)
    // toggleState == false → signal active  → UP   frame (LED on)
    const int frame = button.getToggleState() ? 1 : 0;

    auto bounds = button.getLocalBounds();
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(switchStrip,
                bounds.getX(), bounds.getY(),
                bounds.getWidth(), bounds.getHeight(),
                0, frame * frameSize, frameSize, frameSize);
}


// =============================================================================
// ZoneIndicator
// =============================================================================

ZoneIndicator::ZoneIndicator(juce::AudioProcessorValueTreeState& a)
    : apvts(a)
{
    setInterceptsMouseClicks(false, false);
    if (auto* p = apvts.getRawParameterValue("dial"))
        lastDialValue = p->load();
    startTimerHz(30);
}

ZoneIndicator::~ZoneIndicator() = default;

void ZoneIndicator::timerCallback()
{
    if (auto* p = apvts.getRawParameterValue("dial"))
    {
        const float v = p->load();
        if (std::abs(v - lastDialValue) > 0.001f)
        {
            lastDialValue = v;
            repaint();
        }
    }
}

void ZoneIndicator::paint(juce::Graphics& g)
{
    int zoneA = 0, zoneB = 0;
    float blend = 0.0f;
    ZoneBlender::getZoneBlend(lastDialValue, zoneA, zoneB, blend);

    std::array<float, 5> brightness{};
    brightness.fill(0.0f);
    if (zoneA >= 0 && zoneA < 5)
        brightness[(size_t) zoneA] = 1.0f - blend;
    if (zoneB != zoneA && zoneB >= 0 && zoneB < 5)
        brightness[(size_t) zoneB] = blend;

    auto bounds = getLocalBounds().toFloat();
    const float ledRadius = 3.6f;
    const float spacing = bounds.getWidth() / 5.0f;
    const float cy = bounds.getCentreY();

    for (int i = 0; i < 5; ++i)
    {
        const float cx = bounds.getX() + spacing * ((float) i + 0.5f);
        drawLamp(g, cx, cy, ledRadius, brightness[(size_t) i]);
    }
}


// =============================================================================
// TimbroEditor
// =============================================================================

TimbroEditor::TimbroEditor(Timbro& p)
    : juce::AudioProcessorEditor(&p),
      processor(p),
      zoneIndicator(p.getAPVTS())
{
    setSize(kWindowW, kWindowH);
    setLookAndFeel(&lnf);

    panelImage = juce::ImageCache::getFromMemory(BinaryData::panel_png,
                                                   BinaryData::panel_pngSize);

    auto configureKnob = [this](juce::Slider& k)
    {
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(k);
    };

    configureKnob(inputKnob);
    configureKnob(outputKnob);
    configureKnob(voiceKnob);

    bypassButton.setClickingTogglesState(true);
    addAndMakeVisible(bypassButton);

    addAndMakeVisible(zoneIndicator);

    auto& apvts = processor.getAPVTS();
    inputAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "inputGain", inputKnob);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputGain", outputKnob);
    voiceAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "dial", voiceKnob);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "bypass", bypassButton);
}

TimbroEditor::~TimbroEditor()
{
    setLookAndFeel(nullptr);
}

void TimbroEditor::paint(juce::Graphics& g)
{
    if (panelImage.isValid())
    {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
        g.drawImage(panelImage, getLocalBounds().toFloat(),
                    juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        g.fillAll(juce::Colour(0xFF1C1410));
    }
}

void TimbroEditor::resized()
{
    bypassButton.setBounds(ctrlBounds(kBypassCX));
    inputKnob   .setBounds(ctrlBounds(kInputCX));
    outputKnob  .setBounds(ctrlBounds(kOutputCX));
    voiceKnob   .setBounds(ctrlBounds(kVoiceCX));

    zoneIndicator.setBounds(kVoiceCX - kZoneStripW / 2,
                            kZoneStripY,
                            kZoneStripW, kZoneStripH);
}
