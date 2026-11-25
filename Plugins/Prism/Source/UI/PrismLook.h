#pragma once
#include "UI/LookAndFeel.h"

namespace prism::ui
{
// Palette: warm white enclosure, ink text, one saturated colour per module
struct Palette
{
    static juce::Colour page()    { return juce::Colour (0xfff3f1ec); }
    static juce::Colour body()    { return juce::Colour (0xffffffff); }
    static juce::Colour well()    { return juce::Colour (0xfff2f0eb); }
    static juce::Colour line()    { return juce::Colour (0xffe3e0d9); }
    static juce::Colour ink()     { return juce::Colour (0xff1d1e24); }
    static juce::Colour ink2()    { return juce::Colour (0xff6c6f79); }
    static juce::Colour ink3()    { return juce::Colour (0xffa4a6ae); }
    static juce::Colour ice()     { return juce::Colour (0xff4fb7ff); }
    static juce::Colour module (int m)
    {
        static const juce::Colour c[4] = { juce::Colour (0xffff5b4f), juce::Colour (0xff8b5cf6), juce::Colour (0xff10b5a6), juce::Colour (0xfff5a623) };
        return c[juce::jlimit (0, 3, m)];
    }
};

inline polar::Theme lightTheme()
{
    polar::Theme t;
    t.accent = juce::Colour (0xff8b5cf6); t.accentSoft = t.accent.withAlpha (0.18f);
    t.bg0 = Palette::page(); t.bg1 = Palette::body(); t.bg2 = Palette::well();
    t.line = juce::Colour (0x1a000000); t.text = Palette::ink(); t.textDim = Palette::ink2(); t.textFaint = Palette::ink3();
    return t;
}

class PrismLook : public polar::LookAndFeel
{
public:
    PrismLook() : polar::LookAndFeel (lightTheme())
    {
        setColour (juce::PopupMenu::backgroundColourId, Palette::body());
        setColour (juce::PopupMenu::textColourId, Palette::ink());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::module (1).withAlpha (0.15f));
        setColour (juce::PopupMenu::highlightedTextColourId, Palette::ink());
        setColour (juce::ComboBox::textColourId, Palette::ink());
        setColour (juce::ComboBox::backgroundColourId, Palette::well());
        setColour (juce::ComboBox::arrowColourId, Palette::ink2());
        setColour (juce::Label::textColourId, Palette::ink());
        setColour (juce::TextButton::textColourOffId, Palette::ink());
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Slider::textBoxTextColourId, Palette::ink2());
    }

    // White knob cap on a soft shadow, grey track, saturated value arc in the module colour
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto c = bounds.getCentre();
        const float r = size * 0.5f;
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);
        const float angle = a0 + pos * (a1 - a0);
        const bool bipolar = s.getProperties()["bipolar"];
        const float trackW = juce::jmax (3.0f, r * 0.12f), arcR = r - trackW * 0.5f;

        juce::Path track; track.addCentredArc (c.x, c.y, arcR, arcR, 0, a0, a1, true);
        g.setColour (Palette::line());
        g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        juce::Path arc; arc.addCentredArc (c.x, c.y, arcR, arcR, 0, juce::jmin (from, angle), juce::jmax (from, angle), true);
        if (s.isMouseOverOrDragging())
        {
            g.setColour (accent.withAlpha (0.22f));
            g.strokePath (arc, juce::PathStrokeType (trackW * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (trackW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float capR = r - trackW * 2.3f;
        juce::Path cap; cap.addEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
        juce::DropShadow (juce::Colours::black.withAlpha (0.16f), (int) (capR * 0.5f), { 0, (int) (capR * 0.18f) }).drawForPath (g, cap);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x, c.y - capR, juce::Colour (0xffeceae5), c.x, c.y + capR, false));
        g.fillPath (cap);
        g.setColour (juce::Colour (0x14000000)); g.strokePath (cap, juce::PathStrokeType (1.0f));
        // knurled rim ticks
        for (int i = 0; i < 24; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
            auto p1 = c.getPointOnCircumference (capR * 0.86f, a), p2 = c.getPointOnCircumference (capR * 0.97f, a);
            g.setColour (juce::Colour (0x0d000000)); g.drawLine ({ p1, p2 }, 1.0f);
        }
        auto dot = c.getPointOnCircumference (capR * 0.62f, angle);
        const float dr = juce::jmax (2.5f, capR * 0.13f);
        g.setColour (Palette::ink()); g.fillEllipse (dot.x - dr, dot.y - dr, dr * 2, dr * 2);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        auto track = b.withSizeKeepingCentre (b.getWidth(), 8.0f);
        juce::ColourGradient grad (Palette::module (0), track.getX(), 0, Palette::module (3), track.getRight(), 0, false);
        grad.addColour (0.35, Palette::module (1)); grad.addColour (0.7, Palette::module (2));
        g.setColour (Palette::line()); g.fillRoundedRectangle (track, 4.0f);
        g.setGradientFill (grad); g.fillRoundedRectangle (track.withRight (pos), 4.0f);
        juce::Path knob; knob.addEllipse (pos - 10, track.getCentreY() - 10, 20, 20);
        juce::DropShadow (juce::Colours::black.withAlpha (0.2f), 6, { 0, 2 }).drawForPath (g, knob);
        g.setColour (juce::Colours::white); g.fillPath (knob);
        g.setColour (Palette::ink()); g.fillEllipse (pos - 3, track.getCentreY() - 3, 6, 6);
        juce::ignoreUnused (s);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        auto accent = b.findColour (juce::TextButton::buttonOnColourId);
        auto base = on ? accent : Palette::well();
        if (over) base = base.darker (0.04f);
        if (down) base = base.darker (0.1f);
        g.setColour (base); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        if (! on) { g.setColour (Palette::line()); g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f); }
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return polar::uiFont (juce::jmin (13.0f, h * 0.5f), true); }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (Palette::well()); g.fillRoundedRectangle (r, h * 0.5f);
        g.setColour (box.isMouseOver() ? Palette::ink3() : Palette::line()); g.drawRoundedRectangle (r, h * 0.5f, 1.0f);
        juce::Path ch; const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
        ch.startNewSubPath (cx - 4, cy - 2); ch.lineTo (cx, cy + 2); ch.lineTo (cx + 4, cy - 2);
        g.setColour (Palette::ink2()); g.strokePath (ch, juce::PathStrokeType (1.6f));
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (14, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (polar::uiFont (13.0f, true));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (Palette::body());
        g.setColour (Palette::line()); g.drawRect (0, 0, w, h, 1);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        auto sw = juce::Rectangle<float> (r.getX(), r.getCentreY() - 10, 36, 20);
        const bool on = b.getToggleState();
        auto accent = b.findColour (juce::ToggleButton::tickColourId);
        g.setColour (on ? accent : Palette::line().darker (over ? 0.08f : 0.0f));
        g.fillRoundedRectangle (sw, 10.0f);
        juce::Path k; k.addEllipse (on ? sw.getRight() - 18 : sw.getX() + 2, sw.getY() + 2, 16, 16);
        juce::DropShadow (juce::Colours::black.withAlpha (0.2f), 3, { 0, 1 }).drawForPath (g, k);
        g.setColour (juce::Colours::white); g.fillPath (k);
        g.setColour (Palette::ink2()); g.setFont (polar::uiFont (12.0f, true));
        g.drawText (b.getButtonText(), (int) sw.getRight() + 8, (int) r.getY(), (int) r.getWidth() - 44, (int) r.getHeight(), juce::Justification::centredLeft);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (polar::uiFont (11.5f, true));
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, Palette::ink2());
        l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour (juce::Label::backgroundWhenEditingColourId, Palette::well());
        l->setColour (juce::Label::textWhenEditingColourId, Palette::ink());
        l->setColour (juce::TextEditor::highlightColourId, Palette::module (1).withAlpha (0.25f));
        return l;
    }
};
} // namespace prism::ui
