#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ZoneBlender.h"

namespace
{
    // === Vintage Color Palette ===
    const juce::Colour kWalnut{0xFF2A1F17};
    const juce::Colour kMahogany{0xFF3D2B1F};
    const juce::Colour kPanelDark{0xFF1C1410};
    const juce::Colour kFaceplate{0xFF2E2620};

    const juce::Colour kIvory{0xFFF5E6C8};
    const juce::Colour kIvoryDim{0xFFC4B08A};

    const juce::Colour kAmber{0xFFE8A430};
    const juce::Colour kAmberDim{0xFF8B6914};
    const juce::Colour kAmberGlow{0xFFFFBE44};
    const juce::Colour kBrass{0xFFB8963E};
    const juce::Colour kBrassDark{0xFF7A6428};

    const juce::Colour kKnobDark{0xFF1A1612};
    const juce::Colour kKnobMid{0xFF2E2822};
    const juce::Colour kKnobLight{0xFF3E3630};
    const juce::Colour kKnobEdge{0xFF4A4038};

    const juce::Colour kVURed{0xFFCC2222};

    const juce::Colour kLampOff{0xFF3A2E22};
    const juce::Colour kLampGreen{0xFF44BB44};
    const juce::Colour kLampYellow{0xFFDDCC22};
    const juce::Colour kLampOrange{0xFFEE8822};
    const juce::Colour kLampRed{0xFFDD4422};
    const juce::Colour kLampDeepRed{0xFFCC2222};

    const juce::Colour kZoneColors[] = {kLampGreen, kLampYellow, kLampOrange, kLampRed, kLampDeepRed};

    void drawScrew(juce::Graphics& g, float cx, float cy, float size)
    {
        juce::ColourGradient screwGrad(kBrass.brighter(0.2f), cx - size * 0.3f, cy - size * 0.3f,
                                        kBrassDark, cx + size * 0.3f, cy + size * 0.3f, true);
        g.setGradientFill(screwGrad);
        g.fillEllipse(cx - size, cy - size, size * 2.0f, size * 2.0f);
        g.setColour(kBrassDark.darker(0.4f));
        g.drawLine(cx - size * 0.5f, cy, cx + size * 0.5f, cy, 1.2f);
    }

    void drawBeveledPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float cornerSize,
                           juce::Colour fill, float bevelDepth = 1.5f)
    {
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillRoundedRectangle(bounds.translated(bevelDepth, bevelDepth), cornerSize);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, cornerSize);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), cornerSize, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.15f));
        g.drawRoundedRectangle(bounds.reduced(1.5f), cornerSize - 0.5f, 1.0f);
    }

    // Layout constants
    constexpr float kWindowW = 460.0f;
    constexpr float kWindowH = 600.0f;
    constexpr float kTitleH = 70.0f;
    constexpr float kStripH = 36.0f;
    constexpr float kStripSpacer = 16.0f;
    constexpr float kDialH = 290.0f;
    constexpr float kLabelH = 30.0f;
    constexpr float kBottomSpacer = 16.0f;
    constexpr float kBottomH = 120.0f;     // more room for IN/OUT knobs
    constexpr float kDialInsetX = 85.0f;
    constexpr float kDialInsetY = 35.0f;
}

// ============================================================
// VintageLookAndFeel
// ============================================================

VintageLookAndFeel::VintageLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, kIvory);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ToggleButton::textColourId, kIvory.withAlpha(0.6f));
    setColour(juce::ToggleButton::tickColourId, kAmber);
}

void VintageLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    const float radius = static_cast<float>(juce::jmin(width, height)) / 2.0f - 4.0f;
    const float centreX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool isMainDial = (slider.getName() == "MainDial");

    // === Drop shadow ===
    if (isMainDial)
    {
        for (float s = 8.0f; s >= 2.0f; s -= 2.0f)
        {
            g.setColour(juce::Colours::black.withAlpha(0.08f));
            g.fillEllipse(centreX - radius - s, centreY - radius - s + 2,
                          (radius + s) * 2.0f, (radius + s) * 2.0f);
        }
    }

    // === Outer knurled ring (main dial only) ===
    if (isMainDial)
    {
        float outerR = radius + 2.0f;
        g.setColour(kKnobEdge);
        g.fillEllipse(centreX - outerR, centreY - outerR, outerR * 2.0f, outerR * 2.0f);

        g.setColour(kKnobDark.withAlpha(0.7f));
        for (int i = 0; i < 60; ++i)
        {
            float a = static_cast<float>(i) / 60.0f * juce::MathConstants<float>::twoPi;
            float x1 = centreX + std::cos(a) * (outerR - 3.0f);
            float y1 = centreY + std::sin(a) * (outerR - 3.0f);
            float x2 = centreX + std::cos(a) * outerR;
            float y2 = centreY + std::sin(a) * outerR;
            g.drawLine(x1, y1, x2, y2, 0.8f);
        }
    }

    // === Knob body ===
    {
        juce::ColourGradient bodyGrad(kKnobLight, centreX, centreY - radius * 0.7f,
                                       kKnobDark, centreX, centreY + radius * 0.8f, false);
        bodyGrad.addColour(0.4, kKnobMid);
        g.setGradientFill(bodyGrad);
        float bodyR = isMainDial ? radius - 1.0f : radius;
        g.fillEllipse(centreX - bodyR, centreY - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    }

    // === Concentric grooves (main dial only) ===
    if (isMainDial)
    {
        for (float r = radius * 0.25f; r < radius * 0.88f; r += 3.5f)
        {
            g.setColour(juce::Colours::black.withAlpha(0.04f));
            g.drawEllipse(centreX - r, centreY - r, r * 2.0f, r * 2.0f, 0.6f);
            g.setColour(juce::Colours::white.withAlpha(0.015f));
            g.drawEllipse(centreX - r - 0.5f, centreY - r - 0.5f, r * 2.0f + 1.0f, r * 2.0f + 1.0f, 0.3f);
        }
    }

    // === Arc track ===
    // For small knobs: keep arc INSIDE bounds (radius - 2), for main dial: outside (radius + 10)
    {
        float arcR = isMainDial ? radius + 10.0f : radius - 2.0f;
        juce::Path arcBg;
        arcBg.addCentredArc(centreX, centreY, arcR, arcR,
                            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(kPanelDark.brighter(0.05f));
        g.strokePath(arcBg, juce::PathStrokeType(isMainDial ? 4.0f : 2.5f));
    }

    // === Arc fill (amber) ===
    {
        float arcR = isMainDial ? radius + 10.0f : radius - 2.0f;
        juce::Path arcFill;
        arcFill.addCentredArc(centreX, centreY, arcR, arcR,
                              0.0f, rotaryStartAngle, angle, true);
        g.setColour(isMainDial ? kAmber : kAmberDim);
        g.strokePath(arcFill, juce::PathStrokeType(isMainDial ? 4.0f : 2.5f));

        if (isMainDial)
        {
            juce::Path glowArc;
            glowArc.addCentredArc(centreX, centreY, arcR, arcR,
                                   0.0f, rotaryStartAngle, angle, true);
            g.setColour(kAmberGlow.withAlpha(0.15f));
            g.strokePath(glowArc, juce::PathStrokeType(8.0f));
        }
    }

    // === Pointer ===
    {
        const float pointerLen = isMainDial ? radius * 0.72f : radius * 0.65f;
        const float pointerW = isMainDial ? 3.5f : 2.0f;

        if (isMainDial)
        {
            juce::Path shadow;
            shadow.addRectangle(-pointerW * 0.5f - 0.5f, -pointerLen, pointerW + 1.0f, pointerLen);
            shadow.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX + 1, centreY + 1));
            g.setColour(juce::Colours::black.withAlpha(0.3f));
            g.fillPath(shadow);
        }

        juce::Path pointer;
        pointer.addRectangle(-pointerW * 0.5f, -pointerLen, pointerW, pointerLen);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(kIvory);
        g.fillPath(pointer);

        float dotR = isMainDial ? 3.5f : 2.0f;
        float dotX = centreX + std::sin(angle) * (pointerLen - dotR);
        float dotY = centreY - std::cos(angle) * (pointerLen - dotR);
        g.setColour(kAmberGlow);
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
    }

    // === Center cap ===
    {
        float capR = isMainDial ? radius * 0.17f : radius * 0.22f;

        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillEllipse(centreX - capR + 1, centreY - capR + 1, capR * 2.0f, capR * 2.0f);

        juce::ColourGradient capGrad(kBrass.brighter(0.15f), centreX, centreY - capR,
                                      kBrassDark, centreX, centreY + capR, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);

        g.setColour(kBrass.withAlpha(0.5f));
        g.drawEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f, 1.0f);

        float dotR = capR * 0.3f;
        g.setColour(kBrassDark.darker(0.3f));
        g.fillEllipse(centreX - dotR, centreY - dotR, dotR * 2.0f, dotR * 2.0f);
    }
}

void VintageLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                            bool /*shouldDrawButtonAsHighlighted*/,
                                            bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto centre = bounds.getCentre();

    float lampSize = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.22f;
    float lampY = centre.y - 8.0f;

    // Lamp housing
    g.setColour(kKnobEdge);
    g.fillEllipse(centre.x - lampSize - 2, lampY - lampSize - 2,
                  (lampSize + 2) * 2.0f, (lampSize + 2) * 2.0f);

    bool isOn = button.getToggleState();
    if (isOn)
    {
        g.setColour(kVURed.withAlpha(0.3f));
        g.fillEllipse(centre.x - lampSize - 4, lampY - lampSize - 4,
                      (lampSize + 4) * 2.0f, (lampSize + 4) * 2.0f);
        juce::ColourGradient lampGrad(kVURed.brighter(0.4f), centre.x, lampY - lampSize * 0.5f,
                                       kVURed.darker(0.2f), centre.x, lampY + lampSize * 0.5f, false);
        g.setGradientFill(lampGrad);
    }
    else
    {
        g.setColour(kLampGreen.withAlpha(0.2f));
        g.fillEllipse(centre.x - lampSize - 3, lampY - lampSize - 3,
                      (lampSize + 3) * 2.0f, (lampSize + 3) * 2.0f);
        juce::ColourGradient lampGrad(kLampGreen.brighter(0.3f), centre.x, lampY - lampSize * 0.5f,
                                       kLampGreen.darker(0.3f), centre.x, lampY + lampSize * 0.5f, false);
        g.setGradientFill(lampGrad);
    }
    g.fillEllipse(centre.x - lampSize, lampY - lampSize, lampSize * 2.0f, lampSize * 2.0f);

    // Highlight
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    float spotR = lampSize * 0.35f;
    g.fillEllipse(centre.x - spotR * 0.5f - lampSize * 0.2f,
                  lampY - spotR * 0.5f - lampSize * 0.3f, spotR, spotR);

    // Label
    g.setColour(kIvory.withAlpha(0.5f));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText(button.getButtonText(),
               bounds.withTop(lampY + lampSize + 4).withHeight(14.0f),
               juce::Justification::centredTop);
}

// ============================================================
// Zone Strip
// ============================================================

ZoneStrip::ZoneStrip() {}

void ZoneStrip::setDialValue(float value)
{
    dialValue = value;
    repaint();
}

void ZoneStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float zoneW = bounds.getWidth() / 5.0f;

    int activeZone, blendZone;
    float blendFactor;
    ZoneBlender::getZoneBlend(dialValue, activeZone, blendZone, blendFactor);

    for (int i = 0; i < 5; ++i)
    {
        auto zoneBounds = juce::Rectangle<float>(bounds.getX() + i * zoneW + 2.0f,
                                                  bounds.getY() + 2.0f,
                                                  zoneW - 4.0f,
                                                  bounds.getHeight() - 4.0f);

        g.setColour(kPanelDark);
        g.fillRoundedRectangle(zoneBounds, 3.0f);

        float brightness = 0.0f;
        if (i == activeZone) brightness = 1.0f - blendFactor;
        else if (i == blendZone) brightness = blendFactor;

        auto lampArea = zoneBounds.reduced(3.0f, 3.0f);
        auto lampTop = lampArea.removeFromTop(lampArea.getHeight() * 0.45f);

        if (brightness > 0.05f)
        {
            auto lampColor = kZoneColors[i];
            g.setColour(lampColor.withAlpha(brightness * 0.2f));
            g.fillRoundedRectangle(lampTop.expanded(2.0f), 3.0f);

            juce::ColourGradient lampGrad(lampColor.brighter(0.2f).withAlpha(brightness),
                                           lampTop.getCentreX(), lampTop.getY(),
                                           lampColor.darker(0.1f).withAlpha(brightness * 0.8f),
                                           lampTop.getCentreX(), lampTop.getBottom(), false);
            g.setGradientFill(lampGrad);
            g.fillRoundedRectangle(lampTop, 2.0f);

            g.setColour(juce::Colours::white.withAlpha(brightness * 0.3f));
            auto spotArea = lampTop.reduced(lampTop.getWidth() * 0.25f, lampTop.getHeight() * 0.3f)
                                   .withY(lampTop.getY() + 1);
            g.fillRoundedRectangle(spotArea, 1.5f);
        }
        else
        {
            g.setColour(kLampOff);
            g.fillRoundedRectangle(lampTop, 2.0f);
        }

        g.setColour(brightness > 0.3f ? kIvory.withAlpha(0.9f) : kIvoryDim.withAlpha(0.3f));
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(kZones[static_cast<size_t>(i)].name, lampArea, juce::Justification::centredBottom);

        g.setColour(kKnobEdge.withAlpha(0.3f));
        g.drawRoundedRectangle(zoneBounds, 3.0f, 0.5f);
    }
}

// ============================================================
// Editor
// ============================================================

TimbroEditor::TimbroEditor(Timbro& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(static_cast<int>(kWindowW), static_cast<int>(kWindowH));
    setLookAndFeel(&vintageLnf);

    // Main dial
    dialKnob.setName("MainDial");
    dialKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    dialKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    dialKnob.onValueChange = [this] { updateZoneLabel(); };
    addAndMakeVisible(dialKnob);

    // Zone strip
    addAndMakeVisible(zoneStrip);

    // Zone label
    zoneLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    zoneLabel.setColour(juce::Label::textColourId, kAmber);
    zoneLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(zoneLabel);

    // Input knob
    inputKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    inputKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(inputKnob);

    inputLabel.setText("INPUT", juce::dontSendNotification);
    inputLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    inputLabel.setColour(juce::Label::textColourId, kIvoryDim.withAlpha(0.5f));
    inputLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inputLabel);

    // Output knob
    outputKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    outputKnob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(outputKnob);

    outputLabel.setText("OUTPUT", juce::dontSendNotification);
    outputLabel.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    outputLabel.setColour(juce::Label::textColourId, kIvoryDim.withAlpha(0.5f));
    outputLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputLabel);

    // Bypass
    addAndMakeVisible(bypassButton);

    // APVTS attachments
    dialAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "dial", dialKnob);
    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "inputGain", inputKnob);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.getAPVTS(), "outputGain", outputKnob);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.getAPVTS(), "bypass", bypassButton);

    updateZoneLabel();
}

TimbroEditor::~TimbroEditor()
{
    setLookAndFeel(nullptr);
}

void TimbroEditor::paint(juce::Graphics& g)
{
    auto w = static_cast<float>(getWidth());
    auto h = static_cast<float>(getHeight());

    const float dialTop = kTitleH + kStripH + kStripSpacer;
    const float dialBottom = dialTop + kDialH;

    // === Background — walnut panel with grain ===
    {
        juce::ColourGradient bg(kWalnut.brighter(0.08f), 0, 0,
                                 kWalnut.darker(0.05f), 0, h, false);
        bg.addColour(0.3, kMahogany.withAlpha(0.6f));
        bg.addColour(0.7, kWalnut);
        g.setGradientFill(bg);
        g.fillAll();

        for (float yy = 0; yy < h; yy += 2.5f)
        {
            float alpha = 0.025f + 0.01f * std::sin(yy * 0.7f);
            g.setColour(juce::Colours::black.withAlpha(alpha));
            g.drawLine(0, yy, w, yy, 0.4f);
        }

        float vigSize = juce::jmax(w, h) * 0.7f;
        juce::ColourGradient vignette(juce::Colours::transparentBlack, w * 0.5f, h * 0.4f,
                                       juce::Colours::black.withAlpha(0.2f), w * 0.5f, h * 0.4f + vigSize, true);
        g.setGradientFill(vignette);
        g.fillRect(0.0f, 0.0f, w, h);
    }

    // === Chassis border ===
    g.setColour(kPanelDark);
    g.drawRect(0.0f, 0.0f, w, h, 3.0f);
    g.setColour(kBrass.withAlpha(0.15f));
    g.drawRect(2.0f, 2.0f, w - 4.0f, h - 4.0f, 1.0f);

    // === Corner screws ===
    drawScrew(g, 12.0f, 12.0f, 4.5f);
    drawScrew(g, w - 12.0f, 12.0f, 4.5f);
    drawScrew(g, 12.0f, h - 12.0f, 4.5f);
    drawScrew(g, w - 12.0f, h - 12.0f, 4.5f);

    // === Top nameplate ===
    auto nameplateArea = juce::Rectangle<float>(40.0f, 20.0f, w - 80.0f, 42.0f);
    drawBeveledPanel(g, nameplateArea, 4.0f, kFaceplate.brighter(0.05f), 2.0f);

    g.setColour(kBrass.withAlpha(0.3f));
    g.drawRoundedRectangle(nameplateArea.reduced(1.0f), 3.5f, 1.0f);

    drawScrew(g, nameplateArea.getX() + 10, nameplateArea.getCentreY(), 3.0f);
    drawScrew(g, nameplateArea.getRight() - 10, nameplateArea.getCentreY(), 3.0f);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    g.drawText("TIMBRO", nameplateArea.translated(0.8f, 0.8f), juce::Justification::centred);
    g.setColour(kIvory.withAlpha(0.9f));
    g.drawText("TIMBRO", nameplateArea, juce::Justification::centred);

    g.setColour(kIvoryDim.withAlpha(0.3f));
    g.setFont(juce::FontOptions(8.0f));
    g.drawText("NEURAL AMP MODELER", nameplateArea.withTop(nameplateArea.getBottom() - 14),
               juce::Justification::centred);

    // === Dial section panel ===
    auto dialPanelArea = juce::Rectangle<float>(24.0f, dialTop - 6.0f, w - 48.0f, kDialH + 12.0f);
    drawBeveledPanel(g, dialPanelArea, 6.0f, kPanelDark.brighter(0.02f));

    // === Tick marks around dial ===
    float dialCentreX = w * 0.5f;
    float dialCentreY = dialTop + kDialH * 0.5f;
    float knobW = w - 2.0f * kDialInsetX;
    float knobH2 = kDialH - 2.0f * kDialInsetY;
    float knobRadius = static_cast<float>(juce::jmin(static_cast<int>(knobW), static_cast<int>(knobH2))) / 2.0f - 4.0f;
    float tickRadius = knobRadius + 14.0f;

    float startAngle = juce::MathConstants<float>::pi * 1.25f;
    float range = juce::MathConstants<float>::pi * 1.5f;

    for (int i = 0; i <= 10; ++i)
    {
        float norm = static_cast<float>(i) / 10.0f;
        float angle = startAngle + norm * range;

        bool isMajor = (i % 2 == 0);
        float tickLen = isMajor ? 10.0f : 5.0f;

        float x1 = dialCentreX + std::cos(angle) * tickRadius;
        float y1 = dialCentreY + std::sin(angle) * tickRadius;
        float x2 = dialCentreX + std::cos(angle) * (tickRadius + tickLen);
        float y2 = dialCentreY + std::sin(angle) * (tickRadius + tickLen);

        g.setColour(isMajor ? kIvory.withAlpha(0.6f) : kIvoryDim.withAlpha(0.25f));
        g.drawLine(x1, y1, x2, y2, isMajor ? 1.5f : 1.0f);

        if (isMajor)
        {
            float labelR = tickRadius + tickLen + 10.0f;
            float lx = dialCentreX + std::cos(angle) * labelR;
            float ly = dialCentreY + std::sin(angle) * labelR;
            g.setColour(kIvory.withAlpha(0.5f));
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(juce::String(i), static_cast<int>(lx) - 12, static_cast<int>(ly) - 7, 24, 14,
                       juce::Justification::centred);
        }
    }

    // === Bottom section brass trim ===
    float bottomSepY = dialBottom + kLabelH + kBottomSpacer * 0.5f;
    g.setColour(kBrass.withAlpha(0.12f));
    g.drawLine(30.0f, bottomSepY, w - 30.0f, bottomSepY, 1.0f);

    g.setColour(kBrass.withAlpha(0.06f));
    g.drawLine(30.0f, h - 20.0f, w - 30.0f, h - 20.0f, 0.5f);
}

void TimbroEditor::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop(static_cast<int>(kTitleH));

    auto stripArea = bounds.removeFromTop(static_cast<int>(kStripH));
    zoneStrip.setBounds(stripArea.reduced(30, 0));

    bounds.removeFromTop(static_cast<int>(kStripSpacer));

    auto dialArea = bounds.removeFromTop(static_cast<int>(kDialH));
    dialKnob.setBounds(dialArea.reduced(static_cast<int>(kDialInsetX), static_cast<int>(kDialInsetY)));

    auto labelArea = bounds.removeFromTop(static_cast<int>(kLabelH));
    zoneLabel.setBounds(labelArea);

    bounds.removeFromTop(static_cast<int>(kBottomSpacer));

    // Bottom controls — 3 columns with generous knob space
    auto bottomArea = bounds.removeFromTop(static_cast<int>(kBottomH));

    int thirdW = bottomArea.getWidth() / 3;
    auto leftCol = bottomArea.removeFromLeft(thirdW);
    auto rightCol = bottomArea.removeFromRight(thirdW);
    auto centerCol = bottomArea;

    // Input knob — square bounds so arc doesn't clip
    auto inputKnobArea = leftCol.removeFromTop(leftCol.getHeight() - 20);
    int knobSize = juce::jmin(inputKnobArea.getWidth() - 16, inputKnobArea.getHeight() - 8);
    inputKnob.setBounds(inputKnobArea.withSizeKeepingCentre(knobSize, knobSize));
    inputLabel.setBounds(leftCol);

    // Output knob
    auto outputKnobArea = rightCol.removeFromTop(rightCol.getHeight() - 20);
    knobSize = juce::jmin(outputKnobArea.getWidth() - 16, outputKnobArea.getHeight() - 8);
    outputKnob.setBounds(outputKnobArea.withSizeKeepingCentre(knobSize, knobSize));
    outputLabel.setBounds(rightCol);

    // Bypass
    bypassButton.setBounds(centerCol.reduced(20, 8));
}

void TimbroEditor::updateZoneLabel()
{
    float dialValue = static_cast<float>(dialKnob.getValue());
    zoneLabel.setText(ZoneBlender::getZoneName(dialValue), juce::dontSendNotification);
    zoneStrip.setDialValue(dialValue);
}
