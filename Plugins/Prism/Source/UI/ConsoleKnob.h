#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Materials.h"
#include "PBR.h"
#include "PrismFonts.h"
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
        readout.setFont (fonts::mono (11.0f));
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
    void setAccent (juce::Colour c) { accent = c; repaint(); }

    // ---------------------------------------------------------------------------------------------
    void resized() override
    {
        auto r = getLocalBounds();
        readout.setBounds (r.removeFromBottom (18));
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        r.removeFromBottom (18);
        auto titleArea = r.removeFromTop (16);
        mat::engraved (g, title.toUpperCase(), titleArea, fonts::label (size == Size::Large ? 14.0f : 12.5f), juce::Colour (0xff2b2d33));

        const float dia = juce::jmin (r.getWidth(), r.getHeight()) - 6.0f;
        knobArea = juce::Rectangle<float> (dia, dia).withCentre (r.getCentre());
        const auto c = knobArea.getCentre();
        const float R = dia * 0.5f;
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        const float angle = a0 + value * (a1 - a0);
        const float skirtR = R * 0.80f, capR = skirtR * 0.70f;

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

        // physically rendered body: lighting is fixed, the knurl frame is picked from the current angle
        {
            const int bodyPx = juce::roundToInt (skirtR * 2.0f * 2.0f);   // rendered at 2x for crisp edges
            const auto& frames = pbr::knobFrames (bodyPx);
            const float period = juce::MathConstants<float>::twoPi / (float) pbr::kKnurl;
            float ph = std::fmod (angle, period); if (ph < 0) ph += period;
            const int fi = juce::jlimit (0, pbr::kFrames - 1, (int) (ph / period * (float) pbr::kFrames));
            const float drawn = skirtR * 2.0f * 1.3f;
            g.setOpacity (1.0f);
            g.drawImage (frames[(size_t) fi], juce::Rectangle<float> (drawn, drawn).withCentre (c), juce::RectanglePlacement::stretchToFit);
        }

        // engraved, paint-filled indicator groove: dark top-left wall, lit bottom-right wall, accent fill
        const auto i1 = c.getPointOnCircumference (capR * 0.26f, angle), i2 = c.getPointOnCircumference (capR * 0.80f, angle);
        const float gw = juce::jmax (2.0f, dia * 0.04f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.drawLine ({ i1.translated (-0.5f, -0.6f), i2.translated (-0.5f, -0.6f) }, gw + 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.75f)); g.drawLine ({ i1.translated (0.6f, 0.8f), i2.translated (0.6f, 0.8f) }, gw * 0.9f);
        g.setColour (accent.darker (0.15f)); g.drawLine ({ i1, i2 }, gw);
        g.setColour (accent.brighter (0.35f).withAlpha (0.8f)); g.drawLine ({ i1.translated (-0.25f, -0.35f), i2.translated (-0.25f, -0.35f) }, gw * 0.35f);
        // printed white tick on the skirt
        const auto s1 = c.getPointOnCircumference (skirtR * 0.80f, angle), s2 = c.getPointOnCircumference (skirtR * 0.96f, angle);
        g.setColour (juce::Colours::white.withAlpha (0.9f)); g.drawLine ({ s1, s2 }, juce::jmax (1.5f, dia * 0.025f));

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

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::Label readout;
    juce::Colour accent;
    Size size;
    bool bipolar;
    juce::String title;
    juce::Rectangle<float> knobArea;
    float value = 0, raw = 0, defaultValue = 0, detentFlash = 0;
    bool dragging = false;
    juce::Point<float> lastPos, downScreenPos;
};
} // namespace prism::ui
