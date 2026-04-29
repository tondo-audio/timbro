#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ZoneBlender.h"
#include "BinaryData.h"
#include "timbro_version.h"

#include <array>
#include <cmath>

namespace
{
    // ---- Layout (window 720x280) ------------------------------------------
    constexpr int kWindowW       = 720;
    constexpr int kWindowH       = 280;

    // VOICE is the heart of Timbro — bigger than the trim controls to
    // establish visual hierarchy (the eye lands here first).
    constexpr int kKnobDiameter      = 95;
    constexpr int kVoiceKnobDiameter = 140;
    constexpr int kBypassW           = 80;
    constexpr int kBypassH           = 40;

    constexpr int kCtrlRowCY     = 160;
    constexpr int kBypassCX      = 105;
    constexpr int kInputCX       = 275;
    constexpr int kOutputCX      = 445;
    constexpr int kVoiceCX       = 615;

    constexpr int kTitleY        = 12;
    constexpr int kTitleH        = 28;
    constexpr int kDividerY      = 50;
    constexpr int kLabelTopY     = 64;
    constexpr int kLabelH        = 16;

    constexpr int kValueReadoutY = 218;
    constexpr int kValueReadoutH = 14;

    constexpr int kZoneStripW    = 130;
    constexpr int kZoneStripY    = 240;
    constexpr int kZoneStripH    = 36;

    // ---- Palette: warm-dark, Surge-inspired -------------------------------
    const juce::Colour kBgColour       (0xff1a1612);
    const juce::Colour kPanelDivider   (0xff3a3530);
    const juce::Colour kKnobTrack      (0xff2f2924);
    const juce::Colour kKnobTrackHover (0xff453d36);
    const juce::Colour kKnobInner      (0xff241f1a);
    const juce::Colour kKnobInnerEdge  (0xff3a3530);
    const juce::Colour kAccent         (0xffd97742);
    const juce::Colour kLabelColour    (0xffa89c8a);
    const juce::Colour kTitleColour    (0xffede0c8);
    const juce::Colour kIndicator      (0xffd9ccb8);

    juce::Rectangle<int> knobBounds(int cx, int diameter)
    {
        return { cx - diameter / 2,
                 kCtrlRowCY - diameter / 2,
                 diameter, diameter };
    }

    juce::Rectangle<int> bypassBounds(int cx)
    {
        return { cx - kBypassW / 2,
                 kCtrlRowCY - kBypassH / 2,
                 kBypassW, kBypassH };
    }

    juce::Typeface::Ptr getInterBoldTypeface()
    {
        // Loaded once on first use and cached for the lifetime of the app.
        // All UI text in the editor routes through this face for cross-platform
        // typographic consistency.
        static auto tf = juce::Typeface::createSystemTypefaceFor(
            BinaryData::InterBold_ttf, BinaryData::InterBold_ttfSize);
        return tf;
    }

    juce::Font makeUiFont(float height, float kerning = 0.0f)
    {
        auto f = juce::Font(juce::FontOptions().withTypeface(getInterBoldTypeface())
                                                .withHeight(height));
        if (kerning > 0.0f)
            f.setExtraKerningFactor(kerning);
        return f;
    }
}

// =============================================================================
// TimbroLookAndFeel
// =============================================================================

TimbroLookAndFeel::TimbroLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,        kLabelColour);
    setColour(juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId,         juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::tickColourId,         juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::tickDisabledColourId, juce::Colours::transparentBlack);
}

void TimbroLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, w, h).toFloat().reduced(2.0f);
    const auto centre = bounds.getCentre();

    const float radius        = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float ringThickness = 4.0f;
    const float ringRadius    = radius - ringThickness * 0.5f;
    const float innerRadius   = radius - 12.0f;

    // Hover stays true throughout a drag, even when the cursor leaves the
    // slider's bounds — without this the highlight flickers off mid-gesture.
    const bool hovered = slider.isMouseOver(true) || slider.isMouseButtonDown(true);
    const bool isVoice = slider.getComponentID() == "voice";

    // Track arc (full sweep, dim — slightly brighter on hover)
    juce::Path trackArc;
    trackArc.addCentredArc(centre.x, centre.y, ringRadius, ringRadius,
                           0.0f, startAngle, endAngle, true);
    g.setColour(hovered ? kKnobTrackHover : kKnobTrack);
    g.strokePath(trackArc, juce::PathStrokeType(ringThickness,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::butt));

    // Value arc (start → current, accent)
    const float valueAngle = startAngle + sliderPos * (endAngle - startAngle);
    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, ringRadius, ringRadius,
                           0.0f, startAngle, valueAngle, true);
    g.setColour(hovered ? kAccent.brighter(0.10f) : kAccent);
    g.strokePath(valueArc, juce::PathStrokeType(ringThickness,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::butt));

    // Zone notches: cut bg-coloured slits across the ring at the boundaries
    // between zones (dial values 2/4/6/8 → fractions 0.2/0.4/0.6/0.8).
    // Drawn after the value arc so they remain visible whether the segment
    // is active (amber) or inactive (track-grey).
    if (isVoice)
    {
        g.setColour(kBgColour);
        const float notchInner = ringRadius - ringThickness * 0.5f - 1.0f;
        const float notchOuter = ringRadius + ringThickness * 0.5f + 1.0f;
        for (float frac : { 0.2f, 0.4f, 0.6f, 0.8f })
        {
            const float a = startAngle + frac * (endAngle - startAngle);
            juce::Path t;
            t.startNewSubPath(0.0f, -notchInner);
            t.lineTo(0.0f, -notchOuter);
            t.applyTransform(juce::AffineTransform::rotation(a)
                                 .translated(centre.x, centre.y));
            g.strokePath(t, juce::PathStrokeType(2.0f));
        }
    }

    // Inner disc
    g.setColour(hovered ? kKnobInner.brighter(0.08f) : kKnobInner);
    g.fillEllipse(centre.x - innerRadius, centre.y - innerRadius,
                  innerRadius * 2.0f, innerRadius * 2.0f);
    g.setColour(kKnobInnerEdge);
    g.drawEllipse(centre.x - innerRadius, centre.y - innerRadius,
                  innerRadius * 2.0f, innerRadius * 2.0f, 1.0f);

    // Indicator tick
    juce::Path tick;
    const float tickStart = innerRadius * 0.30f;
    const float tickEnd   = innerRadius * 0.92f;
    tick.startNewSubPath(0.0f, -tickStart);
    tick.lineTo(0.0f, -tickEnd);
    tick.applyTransform(juce::AffineTransform::rotation(valueAngle)
                            .translated(centre.x, centre.y));
    g.setColour(kIndicator);
    g.strokePath(tick, juce::PathStrokeType(2.5f,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

void TimbroLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                          bool /*highlighted*/,
                                          bool /*down*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    const float corner = bounds.getHeight() * 0.5f;

    const bool bypassed = button.getToggleState();
    const bool hovered  = button.isMouseOver(true);

    if (! bypassed)
    {
        g.setColour(hovered ? kAccent.brighter(0.10f) : kAccent);
        g.fillRoundedRectangle(bounds, corner);
        g.setColour(kBgColour);
        g.setFont(makeUiFont(11.0f, 0.10f));
        g.drawText("ON", bounds, juce::Justification::centred);
    }
    else
    {
        g.setColour(hovered ? kKnobTrackHover : kKnobTrack);
        g.fillRoundedRectangle(bounds, corner);
        g.setColour(kPanelDivider);
        g.drawRoundedRectangle(bounds, corner, 1.0f);
        g.setColour(kLabelColour);
        g.setFont(makeUiFont(11.0f, 0.10f));
        g.drawText("OFF", bounds, juce::Justification::centred);
    }
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
    if (zoneA >= 0 && zoneA < 5)
        brightness[(size_t) zoneA] = 1.0f - blend;
    if (zoneB != zoneA && zoneB >= 0 && zoneB < 5)
        brightness[(size_t) zoneB] = blend;

    auto bounds = getLocalBounds().toFloat();

    auto dotsArea = bounds.removeFromTop(14.0f);
    const float dotRadius = 3.0f;
    const float spacing = dotsArea.getWidth() / 5.0f;
    const float dotsCY = dotsArea.getCentreY();

    for (int i = 0; i < 5; ++i)
    {
        const float cx = dotsArea.getX() + spacing * ((float) i + 0.5f);
        const float b = brightness[(size_t) i];
        const auto col = kKnobTrack.interpolatedWith(kAccent, b);
        g.setColour(col);
        g.fillEllipse(cx - dotRadius, dotsCY - dotRadius,
                      dotRadius * 2.0f, dotRadius * 2.0f);
    }

    bounds.removeFromTop(4.0f);
    const int dominant = (blend < 0.5f) ? zoneA : zoneB;
    const char* name = (dominant >= 0 && dominant < 5) ? kZones[(size_t) dominant].name : "";

    g.setFont(makeUiFont(12.0f, 0.10f));
    g.setColour(kAccent);
    g.drawText(name, bounds, juce::Justification::centredTop);
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

    auto configureKnob = [this](HoverableSlider& k)
    {
        k.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(k);
    };

    configureKnob(inputKnob);
    configureKnob(outputKnob);
    configureKnob(voiceKnob);
    voiceKnob.setComponentID("voice");

    bypassButton.setClickingTogglesState(true);
    addAndMakeVisible(bypassButton);

    auto configureValueLabel = [this](juce::Label& l)
    {
        l.setFont(makeUiFont(10.0f, 0.05f));
        l.setColour(juce::Label::textColourId, kIndicator.withAlpha(0.85f));
        l.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l.setJustificationType(juce::Justification::centred);
        l.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
    };

    configureValueLabel(inputValueLabel);
    configureValueLabel(outputValueLabel);

    auto formatDb = [](double v) -> juce::String
    {
        return (v >= 0.0 ? "+" : "") + juce::String(v, 1) + " dB";
    };

    inputKnob.onValueChange = [this, formatDb] {
        inputValueLabel.setText(formatDb(inputKnob.getValue()), juce::dontSendNotification);
    };
    outputKnob.onValueChange = [this, formatDb] {
        outputValueLabel.setText(formatDb(outputKnob.getValue()), juce::dontSendNotification);
    };

    addAndMakeVisible(zoneIndicator);

    auto& apvts = processor.getAPVTS();
    inputAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "inputGain", inputKnob);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputGain", outputKnob);
    voiceAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "dial", voiceKnob);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "bypass", bypassButton);

    // Initial readout sync (attachments fire onValueChange after this).
    inputKnob.onValueChange();
    outputKnob.onValueChange();
}

TimbroEditor::~TimbroEditor()
{
    setLookAndFeel(nullptr);
}

void TimbroEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBgColour);

    g.setColour(kTitleColour);
    g.setFont(makeUiFont(22.0f, 0.18f));
    g.drawText("TIMBRO",
               juce::Rectangle<int>(0, kTitleY, kWindowW, kTitleH),
               juce::Justification::centred);

    g.setColour(kPanelDivider);
    g.fillRect(juce::Rectangle<float>(60.0f, (float) kDividerY,
                                       (float) (kWindowW - 120), 1.0f));

    g.setColour(kLabelColour);
    g.setFont(makeUiFont(11.0f, 0.12f));

    const std::array<std::pair<int, const char*>, 4> labels = {{
        { kBypassCX, "BYPASS" },
        { kInputCX,  "INPUT"  },
        { kOutputCX, "OUTPUT" },
        { kVoiceCX,  "VOICE"  }
    }};

    for (auto& [cx, name] : labels)
    {
        juce::Rectangle<int> r(cx - 60, kLabelTopY, 120, kLabelH);
        g.drawText(name, r, juce::Justification::centred);
    }

    // Version stamp, bottom-right. The macro is injected by CMake from
    // `git describe --tags --dirty`; nothing in the UI needs to be touched
    // when a new tag is cut.
    g.setColour(kPanelDivider.brighter(0.05f));
    g.setFont(makeUiFont(9.0f, 0.05f));
    g.drawText("v" TIMBRO_DISPLAY_VERSION,
               juce::Rectangle<int>(kWindowW - 160, kWindowH - 18, 150, 14),
               juce::Justification::centredRight);
}

void TimbroEditor::resized()
{
    bypassButton.setBounds(bypassBounds(kBypassCX));
    inputKnob   .setBounds(knobBounds(kInputCX,  kKnobDiameter));
    outputKnob  .setBounds(knobBounds(kOutputCX, kKnobDiameter));
    voiceKnob   .setBounds(knobBounds(kVoiceCX,  kVoiceKnobDiameter));

    inputValueLabel .setBounds(kInputCX  - 60, kValueReadoutY, 120, kValueReadoutH);
    outputValueLabel.setBounds(kOutputCX - 60, kValueReadoutY, 120, kValueReadoutH);

    zoneIndicator.setBounds(kVoiceCX - kZoneStripW / 2,
                            kZoneStripY,
                            kZoneStripW, kZoneStripH);
}
