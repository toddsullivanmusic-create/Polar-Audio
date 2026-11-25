#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Materials.h"
#include "UI/Theme.h"

namespace prism::ui
{
// A console-style rotary control, drawn and handled from scratch.
//
// Look: printed scale on the panel, contact shadow, knurled skirt, spun-aluminium cap. The lighting is
//       fixed to the room (top-left) — only the indicator and the knurling rotate, like a real knob.
// Feel: drag up/right to turn (about 1.5 screen-heights for full travel), hold Shift for 8x finer
//       control, bipolar knobs have a centre detent you can feel while dragging, the scroll wheel clicks
//       in 1 % steps, double-click or Option/Alt-click resets, click the readout to type a value.
class ConsoleKnob : public juce::Component, private juce::Timer
{
public:
    enum class Size { Large, Small };

    ConsoleKnob (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& name,
                 juce::Colour accentColour, Size sz = Size::Small, bool bipolarKnob = false)
        : accent (accentColour), size (sz), bipolar (bipolarKnob), title (name)
    {
        param = s.getParameter (paramID);
        jassert (param != nullptr);
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) { setDisplayValue (param->convertTo0to1 (v)); }, s.undoManager);
        attachment->sendInitialUpdate();
        defaultValue = param->getDefaultValue();

        readout.setJustificationType (juce::Justification::centred);
        readout.setEditable (false, true, false);   // double-click the readout to type
        readout.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
        readout.setColour (juce::Label::textColourId, juce::Colour (0xff4a4c54));
        readout.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::white);
        readout.setColour (juce::Label::outlineWhenEditingColourId, accent);
        readout.onTextChange = [this]
        {
            const auto v = param->getValueForText (readout.getText());
            attachment->setValueAsCompleteGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v)));
            updateText();
        };
        readout.setTooltip ("Double-click to type a value");
        addAndMakeVisible (readout);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setRepaintsOnMouseActivity (true);
        updateText();
    }

    void setTitle (const juce::String& t) { if (t != title) { title = t; repaint(); } }
    void setAccent (juce::Colour c) { accent = c; cache = {}; repaint(); }

    // ---------------------------------------------------------------------------------------------
    void resized() override
    {
        auto r = getLocalBounds();
        readout.setBounds (r.removeFromBottom (18));
        cache = {};
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromBottom (18);
        auto titleArea = r.removeFromTop (16);
        mat::engraved (g, title.toUpperCase(), titleArea, juce::Font (juce::FontOptions (size == Size::Large ? 11.5f : 10.5f, juce::Font::bold)), juce::Colour (0xff2b2d33));

        const float dia = juce::jmin (r.getWidth(), r.getHeight()) - 6.0f;
        knobArea = juce::Rectangle<float> (dia, dia).withCentre (r.getCentre());
        const auto c = knobArea.getCentre();
        const float R = dia * 0.5f;
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        const float angle = a0 + value * (a1 - a0);
        const float skirtR = R * 0.80f, capR = R * 0.58f;

        // printed scale on the panel
        const int ticks = size == Size::Large ? 21 : 11;
        for (int i = 0; i < ticks; ++i)
        {
            const float t = (float) i / (float) (ticks - 1);
            const float a = a0 + t * (a1 - a0);
            const bool major = (i % (size == Size::Large ? 5 : 5)) == 0 || i == ticks - 1;
            const float r1 = R * (major ? 0.86f : 0.89f), r2 = R * 0.98f;
            const bool lit = bipolar ? (t >= juce::jmin (0.5f, value) - 1e-4f && t <= juce::jmax (0.5f, value) + 1e-4f) : t <= value + 1e-4f;
            g.setColour (lit ? accent : juce::Colour (0xff6d6f77).withAlpha (major ? 0.8f : 0.45f));
            g.drawLine ({ c.getPointOnCircumference (r1, a), c.getPointOnCircumference (r2, a) }, major ? 1.6f : 1.0f);
        }

        // static body (shadow, skirt, cap, lighting) is cached; it never rotates
        const int scale = 2;
        if (! cache.isValid() || cache.getWidth() != (int) (dia * scale))
            cache = renderBody ((int) (dia * scale), (float) scale);
        g.setOpacity (1.0f); g.drawImage (cache, knobArea.expanded (dia * 0.1f), juce::RectanglePlacement::stretchToFit);

        // knurling on the skirt rotates with the knob
        for (int i = 0; i < 40; ++i)
        {
            const float a = angle + juce::MathConstants<float>::twoPi * (float) i / 40.0f;
            const auto p1 = c.getPointOnCircumference (skirtR * 0.80f, a), p2 = c.getPointOnCircumference (skirtR * 0.985f, a);
            const float lightFacing = 0.5f + 0.5f * std::cos (a + juce::MathConstants<float>::pi * 0.75f);   // brighter toward top-left
            g.setColour (juce::Colours::white.withAlpha (0.06f + 0.22f * lightFacing));
            g.drawLine ({ p1, p2 }, 1.2f);
            g.setColour (juce::Colours::black.withAlpha (0.25f - 0.15f * lightFacing));
            g.drawLine ({ p1.translated (0.9f, 0.9f), p2.translated (0.9f, 0.9f) }, 0.9f);
        }

        // inlaid indicator line, with a hairline shadow so it reads as cut into the cap
        const auto i1 = c.getPointOnCircumference (capR * 0.30f, angle), i2 = c.getPointOnCircumference (capR * 0.93f, angle);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawLine ({ i1.translated (0.7f, 1.0f), i2.translated (0.7f, 1.0f) }, dia * 0.035f);
        g.setColour (accent); g.drawLine ({ i1, i2 }, dia * 0.032f);
        const auto s1 = c.getPointOnCircumference (skirtR * 0.82f, angle), s2 = c.getPointOnCircumference (skirtR * 0.99f, angle);
        g.setColour (juce::Colours::white.withAlpha (0.95f)); g.drawLine ({ s1, s2 }, dia * 0.03f);

        // centre-detent click: the 12 o'clock tick flashes as you pass through it
        if (bipolar && detentFlash > 0.0f)
        {
            const auto t = c.getPointOnCircumference (R * 0.93f, (a0 + a1) * 0.5f);
            g.setColour (accent.withAlpha (detentFlash)); g.fillEllipse (t.x - 3.5f, t.y - 3.5f, 7, 7);
        }
        // touch feedback: a faint accent halo while hovered/dragged
        if (isMouseOverOrDragging())
        {
            g.setColour (accent.withAlpha (dragging ? 0.30f : 0.14f));
            g.drawEllipse (knobArea.reduced (R * 0.15f), dragging ? 2.0f : 1.4f);
        }
    }

    // ---------------------------------------------------------------------------------------------
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isAltDown() || e.mods.isCommandDown()) { resetToDefault(); return; }
        if (e.mods.isPopupMenu()) return;
        dragging = true;
        lastPos = e.position;
        downScreenPos = e.getScreenPosition().toFloat();
        raw = toRaw (value);
        attachment->beginGesture();
        setMouseCursor (juce::MouseCursor::NoCursor);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto d = e.position - lastPos;
        lastPos = e.position;
        const float px = -d.y + d.x * 0.6f;
        const float speed = e.mods.isShiftDown() ? 0.00055f : 0.0045f;     // ~220 px full travel, Shift = 8x finer
        raw = juce::jlimit (0.0f, 1.0f, raw + px * speed);
        const float v = fromRaw (raw);
        if (std::abs (v - value) > 1.0e-6f)
        {
            if (bipolar && (value - 0.5f) * (v - 0.5f) <= 0.0f && v != value) detentFlash = 1.0f;   // crossed the centre click
            attachment->setValueAsPartOfGesture (param->convertFrom0to1 (v));
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragging) return;
        dragging = false;
        attachment->endGesture();
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition (downScreenPos);   // cursor comes back where you grabbed
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override { resetToDefault(); }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const float dir = (w.deltaY != 0 ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
        if (std::abs (dir) < 1.0e-4f) return;
        const float step = e.mods.isShiftDown() ? 0.002f : 0.01f;
        float v = juce::jlimit (0.0f, 1.0f, value + (dir > 0 ? step : -step));
        if (bipolar && std::abs (v - 0.5f) < step * 0.5f) v = 0.5f;
        attachment->setValueAsCompleteGesture (param->convertFrom0to1 (v));
    }

private:
    // Bipolar knobs get a mechanical-feeling detent: the middle 5 % of raw travel all maps to centre
    static constexpr float kDetent = 0.05f;
    float toRaw (float v) const
    {
        if (! bipolar) return v;
        if (std::abs (v - 0.5f) < 1.0e-4f) return 0.5f;
        return v < 0.5f ? v * (1.0f - kDetent) : 0.5f + kDetent * 0.5f + (v - 0.5f) * (1.0f - kDetent);
    }
    float fromRaw (float r) const
    {
        if (! bipolar) return r;
        const float lo = 0.5f - kDetent * 0.5f, hi = 0.5f + kDetent * 0.5f;
        if (r < lo) return r / (1.0f - kDetent);
        if (r > hi) return 0.5f + (r - hi) / (1.0f - kDetent);
        return 0.5f;
    }

    void resetToDefault() { attachment->setValueAsCompleteGesture (param->convertFrom0to1 (defaultValue)); }

    void setDisplayValue (float v01)
    {
        value = v01;
        updateText();
        repaint();
        if (detentFlash > 0) startTimerHz (30);
    }
    void updateText() { readout.setText (param->getCurrentValueAsText(), juce::dontSendNotification); }

    void timerCallback() override
    {
        detentFlash *= 0.7f;
        if (detentFlash < 0.02f) { detentFlash = 0; stopTimer(); }
        repaint();
    }

    juce::Image renderBody (int px, float scale) const
    {
        const float pad = (float) px * 0.1f;   // matches knobArea.expanded (dia * 0.1)
        const int full = (int) (px + pad * 2);
        juce::Image img (juce::Image::ARGB, full, full, true);
        juce::Graphics g (img);
        const juce::Point<float> c ((float) full * 0.5f, (float) full * 0.5f);
        const float R = (float) px * 0.5f;
        const float skirtR = R * 0.80f, capR = R * 0.58f;
        juce::ignoreUnused (scale);

        // contact shadow (soft, offset down-right away from the light)
        for (int i = 0; i < 8; ++i)
        {
            const float rr = skirtR * (1.02f + 0.05f * (float) i);
            g.setColour (juce::Colours::black.withAlpha (0.055f));
            g.fillEllipse (c.x - rr + R * 0.05f, c.y - rr + R * 0.10f, rr * 2, rr * 2);
        }
        // skirt: dark anodised flange
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b4d53), c.x - skirtR, c.y - skirtR, juce::Colour (0xff121316), c.x + skirtR, c.y + skirtR, false));
        g.fillEllipse (c.x - skirtR, c.y - skirtR, skirtR * 2, skirtR * 2);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (c.x - skirtR + 1, c.y - skirtR + 1, skirtR * 2 - 2, skirtR * 2 - 2, 1.2f);
        // cap side wall (gives the cap height)
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9d9d6), c.x - capR, c.y - capR, juce::Colour (0xff6e6f72), c.x + capR, c.y + capR, false));
        g.fillEllipse (c.x - capR * 1.06f, c.y - capR * 1.06f + R * 0.03f, capR * 2.12f, capR * 2.12f);
        // spun-aluminium top
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffefefed), c.x - capR, c.y - capR, juce::Colour (0xffa3a4a6), c.x + capR, c.y + capR, false));
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
        // concentric machining rings
        for (float rr = capR * 0.06f; rr < capR; rr += juce::jmax (1.0f, capR * 0.035f))
        {
            g.setColour ((((int) (rr * 7.3f)) & 1) ? juce::Colours::white.withAlpha (0.06f) : juce::Colours::black.withAlpha (0.035f));
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.8f);
        }
        // anisotropic highlights: the bow-tie reflection a spun finish throws toward the light
        auto petal = [&] (float centreAngle, float alpha)
        {
            juce::Path p;
            p.addPieSegment (c.x - capR, c.y - capR, capR * 2, capR * 2, centreAngle - 0.32f, centreAngle + 0.32f, 0.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (alpha), c.x, c.y,
                                                     juce::Colours::white.withAlpha (0.0f), c.x + std::sin (centreAngle) * capR, c.y - std::cos (centreAngle) * capR, false));
            g.fillPath (p);
        };
        petal (-juce::MathConstants<float>::pi * 0.25f, 0.55f);   // toward top-left
        petal (juce::MathConstants<float>::pi * 0.75f, 0.22f);    // opposite lobe
        // rim bevel
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.9f), c.x - capR, c.y - capR, juce::Colours::black.withAlpha (0.4f), c.x + capR, c.y + capR, false));
        g.drawEllipse (c.x - capR + 0.6f, c.y - capR + 0.6f, capR * 2 - 1.2f, capR * 2 - 1.2f, juce::jmax (1.0f, capR * 0.05f));
        // centre dimple
        const float dr = capR * 0.10f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8f9092), c.x - dr, c.y - dr, juce::Colour (0xfff2f2f0), c.x + dr, c.y + dr, false));
        g.fillEllipse (c.x - dr, c.y - dr, dr * 2, dr * 2);
        return img;
    }

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::Label readout;
    juce::Colour accent;
    Size size;
    bool bipolar;
    juce::String title;
    juce::Image cache;
    juce::Rectangle<float> knobArea;
    float value = 0, raw = 0, defaultValue = 0, detentFlash = 0;
    bool dragging = false;
    juce::Point<float> lastPos, downScreenPos;
};
} // namespace prism::ui
