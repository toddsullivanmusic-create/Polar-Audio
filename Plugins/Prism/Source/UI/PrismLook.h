#pragma once
#include "UI/LookAndFeel.h"
#include "Materials.h"
#include "PBR.h"
#include "PrismFonts.h"

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
        setColour (juce::TextButton::textColourOnId, Palette::ink());
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

    // Hardware fader: recessed slot, machined cap with grip grooves
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        auto slot = b.withSizeKeepingCentre (b.getWidth(), 6.0f);
        g.setColour (juce::Colour (0xff15161a)); g.fillRoundedRectangle (slot, 3.0f);
        mat::innerShadow (g, slot, 3.0f, 4.0f, 0.7f);
        for (int i = 0; i <= 10; ++i)
        {
            const float tx = slot.getX() + slot.getWidth() * (float) i / 10.0f;
            g.setColour (Palette::ink2().withAlpha (i % 5 == 0 ? 0.7f : 0.35f));
            g.drawVerticalLine ((int) tx, slot.getBottom() + 5, slot.getBottom() + (i % 5 == 0 ? 11.0f : 8.0f));
        }
        auto cap = juce::Rectangle<float> (pos - 9, b.getCentreY() - 13, 18, 26);
        juce::Path cp; cp.addRoundedRectangle (cap, 3.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 6, { 2, 4 }).drawForPath (g, cp);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f4f2), cap.getX(), cap.getY(), juce::Colour (0xff9c9d9f), cap.getRight(), cap.getBottom(), false));
        g.fillPath (cp);
        for (int i = 0; i < 4; ++i)
        {
            const float gy = cap.getY() + 6 + i * 4.5f;
            g.setColour (juce::Colours::black.withAlpha (0.3f)); g.drawHorizontalLine ((int) gy, cap.getX() + 3, cap.getRight() - 3);
            g.setColour (juce::Colours::white.withAlpha (0.7f)); g.drawHorizontalLine ((int) gy + 1, cap.getX() + 3, cap.getRight() - 3);
        }
        g.setColour (juce::Colour (0xff1d1e24)); g.fillRect (cap.getCentreX() - 0.75f, cap.getY() + 2, 1.5f, cap.getHeight() - 4);
        mat::bevel (g, cap, 3.0f, 1.2f);
        juce::ignoreUnused (s);
    }

    // Hardware key: raised machined key; when latched it sits pressed in and its LED strip glows
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (3.5f, 3.5f);
        const bool on = b.getToggleState();
        const bool pressed = on || down;
        auto accent = b.findColour (juce::TextButton::buttonOnColourId);
        // physically rendered key, cached per size/state
        g.setOpacity (1.0f);
        g.drawImage (pbr::cachedKey (b.getWidth(), b.getHeight(), pressed, 2.0f), b.getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        if (over && ! pressed) { g.setColour (juce::Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (r, 5.0f); }
        // LED strip
        if (accent != juce::Colours::transparentBlack)
        {
            auto strip = juce::Rectangle<float> (r.getX() + 8, r.getY() + 3, r.getWidth() - 16, 2.5f);
            if (on)
            {
                g.setColour (accent.withAlpha (0.35f)); g.fillRoundedRectangle (strip.expanded (2, 2), 3);
                g.setColour (accent.brighter (0.2f));
            }
            else g.setColour (juce::Colour (0x22000000));
            g.fillRoundedRectangle (strip, 1.25f);
        }
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return fonts::label (juce::jmin (15.0f, h * 0.58f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return fonts::mono (12.0f); }
    juce::Font getPopupMenuFont() override { return fonts::label (15.0f); }
    juce::Font getLabelFont (juce::Label&) override { return fonts::label (13.0f); }

    // Recessed display window
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
        mat::glassBackground (g, r, 6.0f, Palette::module (1));
        juce::Path ch; const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
        ch.startNewSubPath (cx - 4, cy - 2); ch.lineTo (cx, cy + 2); ch.lineTo (cx + 4, cy - 2);
        g.setColour (juce::Colours::white.withAlpha (box.isMouseOver() ? 0.9f : 0.6f)); g.strokePath (ch, juce::PathStrokeType (1.6f));
        mat::glassOverlay (g, r, 6.0f);
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (14, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (fonts::mono (12.0f));
        label.setColour (juce::Label::textColourId, juce::Colour (0xffe8f0ff));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (Palette::body());
        g.setColour (Palette::line()); g.drawRect (0, 0, w, h, 1);
    }

    // Bat-lever toggle switch on a hex nut
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        const auto c = juce::Point<float> (r.getX() + 14, r.getCentreY());
        // hex nut
        juce::Path hex;
        for (int i = 0; i < 6; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 6.0f + 0.52f;
            const juce::Point<float> p (c.x + std::cos (a) * 10.5f, c.y + std::sin (a) * 10.5f);
            i == 0 ? hex.startNewSubPath (p) : hex.lineTo (p);
        }
        hex.closeSubPath();
        juce::DropShadow (juce::Colours::black.withAlpha (0.3f), 4, { 1, 2 }).drawForPath (g, hex);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff1f1ef), c.x - 10, c.y - 10, juce::Colour (0xff8d8e90), c.x + 10, c.y + 10, false));
        g.fillPath (hex);
        g.setColour (juce::Colour (0xff3b3c40)); g.fillEllipse (c.x - 5, c.y - 5, 10, 10);
        // lever (chrome bat) pointing up when on, down when off
        const float dir = on ? -1.0f : 1.0f;
        const auto tip = juce::Point<float> (c.x + 2.0f, c.y + dir * 16.0f);
        juce::Path lever;
        lever.startNewSubPath (c.x - 2.2f, c.y); lever.lineTo (tip.x - 3.2f, tip.y); lever.lineTo (tip.x + 3.2f, tip.y); lever.lineTo (c.x + 2.2f, c.y); lever.closeSubPath();
        juce::DropShadow (juce::Colours::black.withAlpha (0.35f), 3, { 2, 2 }).drawForPath (g, lever);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), tip.x - 3, 0, juce::Colour (0xff777879), tip.x + 3, 0, false));
        g.fillPath (lever);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, tip.x - 3, tip.y - 3, juce::Colour (0xff8a8b8d), tip.x + 3, tip.y + 3, true));
        g.fillEllipse (tip.x - 3.6f, tip.y - 3.6f, 7.2f, 7.2f);
        auto accent = b.findColour (juce::ToggleButton::tickColourId);
        mat::led (g, { r.getX() + 36, r.getCentreY() }, 3.5f, accent, on);
        mat::engraved (g, b.getButtonText().toUpperCase(), { r.getX() + 48, r.getY(), r.getWidth() - 48, r.getHeight() },
                       fonts::label (13.2f), over ? Palette::ink() : Palette::ink2(), juce::Justification::centredLeft);
    }

    juce::Label* createSliderTextBox (juce::Slider& s) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (fonts::label (13.8f));
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
