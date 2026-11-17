#pragma once
#include "Theme.h"

namespace lumen
{
class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit LookAndFeel (Theme t) : theme (t)
    {
        setColour (juce::Slider::textBoxTextColourId, theme.text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::backgroundColourId, theme.bg2);
        setColour (juce::ComboBox::outlineColourId, theme.line);
        setColour (juce::ComboBox::textColourId, theme.text);
        setColour (juce::ComboBox::arrowColourId, theme.textDim);
        setColour (juce::PopupMenu::backgroundColourId, theme.bg1);
        setColour (juce::PopupMenu::textColourId, theme.text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, theme.text);
        setColour (juce::TextButton::buttonColourId, theme.bg2);
        setColour (juce::TextButton::buttonOnColourId, theme.accent);
        setColour (juce::TextButton::textColourOffId, theme.textDim);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, theme.text);
        setColour (juce::ToggleButton::textColourId, theme.textDim);
        setColour (juce::ToggleButton::tickColourId, theme.accent);
    }

    Theme theme;

    juce::Font getLabelFont (juce::Label&) override            { return uiFont (12.5f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override      { return uiFont (13.0f); }
    juce::Font getPopupMenuFont() override                     { return uiFont (13.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return uiFont (12.5f, true); }

    // ---- Rotary: thin track, glowing accent arc, soft inner disc, pointer ----
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        auto size   = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto centre = bounds.getCentre();
        auto r      = size * 0.5f;
        auto angle  = startAngle + pos * (endAngle - startAngle);
        const bool bipolar = s.getProperties()["bipolar"];
        const float trackW = juce::jmax (2.5f, r * 0.11f);
        const float arcR   = r - trackW * 0.5f;

        // Track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (theme.line.withAlpha (0.18f));
        g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value arc (+ glow)
        juce::Path arc;
        float a0 = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        arc.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, juce::jmin (a0, angle), juce::jmax (a0, angle), true);
        auto accent = s.isEnabled() ? theme.accent : theme.accent.withAlpha (0.35f);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (arc, juce::PathStrokeType (trackW * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Inner disc with subtle gradient + rim light
        auto discR = r - trackW * 2.4f;
        juce::ColourGradient grad (theme.bg2.brighter (0.25f), centre.x, centre.y - discR,
                                   theme.bg1.darker (0.2f),    centre.x, centre.y + discR, false);
        g.setGradientFill (grad);
        g.fillEllipse (centre.x - discR, centre.y - discR, discR * 2, discR * 2);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (centre.x - discR, centre.y - discR, discR * 2, discR * 2, 1.0f);

        // Pointer
        juce::Point<float> p1 = centre.getPointOnCircumference (discR * 0.55f, angle);
        juce::Point<float> p2 = centre.getPointOnCircumference (discR * 0.9f, angle);
        g.setColour (theme.text);
        g.drawLine ({ p1, p2 }, juce::jmax (2.0f, discR * 0.09f));
        if (s.isMouseOverOrDragging())
        {
            g.setColour (accent.withAlpha (0.12f));
            g.fillEllipse (centre.x - discR, centre.y - discR, discR * 2, discR * 2);
        }
    }

    // ---- Linear slider: pill track ----
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                           float, float, juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        bool horiz = style == juce::Slider::LinearHorizontal || style == juce::Slider::LinearBar;
        auto track = horiz ? b.withSizeKeepingCentre (b.getWidth(), 6.0f) : b.withSizeKeepingCentre (6.0f, b.getHeight());
        g.setColour (theme.line.withAlpha (0.2f));
        g.fillRoundedRectangle (track, 3.0f);
        auto fill = horiz ? track.withRight (sliderPos) : track.withTop (sliderPos);
        g.setColour (theme.accent);
        g.fillRoundedRectangle (fill, 3.0f);
        auto knob = horiz ? juce::Rectangle<float> (sliderPos - 8, track.getCentreY() - 8, 16, 16)
                          : juce::Rectangle<float> (track.getCentreX() - 8, sliderPos - 8, 16, 16);
        g.setColour (theme.text);
        g.fillEllipse (knob);
        g.setColour (theme.bg0.withAlpha (0.4f));
        g.drawEllipse (knob, 1.0f);
        juce::ignoreUnused (s);
    }

    // ---- Buttons ----
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        bool on = b.getToggleState();
        auto base = on ? theme.accent : theme.bg2;
        if (over) base = base.brighter (0.08f);
        if (down) base = base.darker (0.1f);
        if (on)
        {
            g.setColour (theme.accent.withAlpha (0.35f));
            g.fillRoundedRectangle (r.expanded (2.0f), 10.0f);
        }
        g.setColour (base);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.18f : 0.06f));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        // iOS-style switch
        auto r = b.getLocalBounds().toFloat();
        auto sw = juce::Rectangle<float> (r.getX(), r.getCentreY() - 10, 36, 20);
        bool on = b.getToggleState();
        g.setColour (on ? theme.accent : theme.bg2.brighter (over ? 0.15f : 0.05f));
        g.fillRoundedRectangle (sw, 10.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (on ? sw.getRight() - 18 : sw.getX() + 2, sw.getY() + 2, 16, 16);
        g.setColour (theme.textDim);
        g.setFont (uiFont (12.5f));
        g.drawText (b.getButtonText(), (int) sw.getRight() + 8, (int) r.getY(), (int) r.getWidth() - 44, (int) r.getHeight(),
                    juce::Justification::centredLeft);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (theme.bg2);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (box.isMouseOver() ? 0.14f : 0.07f));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        juce::Path chevron;
        float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
        chevron.startNewSubPath (cx - 4, cy - 2); chevron.lineTo (cx, cy + 2); chevron.lineTo (cx + 4, cy - 2);
        g.setColour (theme.textDim);
        g.strokePath (chevron, juce::PathStrokeType (1.6f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.setColour (theme.bg1);
        g.fillRoundedRectangle (0, 0, (float) w, (float) h, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 10.0f, 1.0f);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (uiFont (11.5f));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, theme.textDim);
        l->setColour (juce::Label::backgroundWhenEditingColourId, theme.bg2);
        l->setColour (juce::Label::textWhenEditingColourId, theme.text);
        return l;
    }
};
} // namespace lumen
