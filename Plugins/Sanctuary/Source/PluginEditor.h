#pragma once
#include "PluginProcessor.h"

// The Key Wheel: 12 segments, click to change key, breathing glow on the active key,
// a progress ring showing the crossfade into the new key, and a centre Play/Stop.
class KeyWheel : public juce::Component, private juce::Timer, private juce::AudioProcessorValueTreeState::Listener
{
public:
    KeyWheel (SanctuaryProcessor& p, const lumen::Theme& t) : proc (p), theme (t)
    {
        proc.apvts.addParameterListener (ID::key, this); proc.apvts.addParameterListener (ID::mode, this); proc.apvts.addParameterListener (ID::play, this);
        startTimerHz (30);
    }
    ~KeyWheel() override { proc.apvts.removeParameterListener (ID::key, this); proc.apvts.removeParameterListener (ID::mode, this); proc.apvts.removeParameterListener (ID::play, this); }

    void paint (juce::Graphics& g) override
    {
        static const char* names[12] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        auto b = getLocalBounds().toFloat();
        auto c = b.getCentre();
        float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.47f;
        float inner = R * 0.62f;
        int key = (int) proc.apvts.getRawParameterValue (ID::key)->load();
        bool minor = proc.apvts.getRawParameterValue (ID::mode)->load() > 0.5f;
        bool playing = proc.apvts.getRawParameterValue (ID::play)->load() > 0.5f;
        float breathe = 0.5f + 0.5f * std::sin (t * 1.4f);

        for (int i = 0; i < 12; ++i)
        {
            float a0 = juce::MathConstants<float>::twoPi * ((float) i - 0.5f) / 12.0f - juce::MathConstants<float>::halfPi;
            float a1 = a0 + juce::MathConstants<float>::twoPi / 12.0f;
            juce::Path seg;
            seg.addPieSegment (c.x - R, c.y - R, R * 2, R * 2, a0 + juce::MathConstants<float>::halfPi, a1 + juce::MathConstants<float>::halfPi, inner / R);
            bool active = i == key, hover = i == hoverIdx;
            auto col = active ? theme.accent.withAlpha (0.55f + 0.3f * breathe * (playing ? 1.0f : 0.3f)) : theme.bg2.withAlpha (hover ? 1.0f : 0.8f);
            g.setColour (col); g.fillPath (seg);
            g.setColour (theme.bg0.withAlpha (0.8f)); g.strokePath (seg, juce::PathStrokeType (2.0f));
            float am = (a0 + a1) * 0.5f;
            auto pt = c.getPointOnCircumference ((R + inner) * 0.5f, am + juce::MathConstants<float>::halfPi);
            g.setColour (active ? juce::Colours::white : theme.textDim);
            g.setFont (lumen::uiFont (active ? 15.0f : 13.0f, active));
            g.drawText (names[i], (int) pt.x - 20, (int) pt.y - 10, 40, 20, juce::Justification::centred);
        }
        // crossfade ring
        float prog = juce::jlimit (0.0f, 1.0f, proc.transition.load());
        juce::Path ring;
        float rr = inner - 6;
        ring.addCentredArc (c.x, c.y, rr, rr, 0, 0, juce::MathConstants<float>::twoPi * prog, true);
        g.setColour (theme.accent.withAlpha (prog < 0.995f ? 0.9f : 0.25f));
        g.strokePath (ring, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // centre button
        float cr = inner - 16;
        g.setColour (playing ? theme.accent.withAlpha (0.25f + 0.15f * breathe) : theme.bg1);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
        g.setColour (juce::Colours::white.withAlpha (0.1f)); g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.0f);
        g.setColour (theme.text); g.setFont (lumen::uiFont (cr * 0.55f, true));
        g.drawText (juce::String (names[key]) + (minor ? "m" : ""), (int) (c.x - cr), (int) (c.y - cr * 0.55f), (int) (cr * 2), (int) (cr * 0.8f), juce::Justification::centred);
        g.setColour (theme.textDim); g.setFont (lumen::uiFont (12.0f, true));
        g.drawText (playing ? juce::String::fromUTF8 ("PLAYING  \xc2\xb7  tap to fade") : juce::String ("tap to play"), (int) (c.x - cr), (int) (c.y + cr * 0.2f), (int) (cr * 2), 20, juce::Justification::centred);
        // swell level arc inside the centre
        float sw = proc.swellLevel.load();
        juce::Path sarc; float sr2 = cr - 6;
        sarc.addCentredArc (c.x, c.y, sr2, sr2, 0, juce::MathConstants<float>::pi * 0.75f, juce::MathConstants<float>::pi * 0.75f + juce::MathConstants<float>::pi * 1.5f * sw, true);
        g.setColour (theme.accent.withAlpha (0.7f)); g.strokePath (sarc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void mouseMove (const juce::MouseEvent& e) override { hoverIdx = segmentAt (e.position); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hoverIdx = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        int idx = segmentAt (e.position);
        if (idx >= 0) { set (ID::key, (float) idx); set (ID::play, 1.0f); }
        else if (idx == -2) set (ID::play, proc.apvts.getRawParameterValue (ID::play)->load() > 0.5f ? 0.0f : 1.0f);
    }
private:
    int segmentAt (juce::Point<float> p) const
    {
        auto b = getLocalBounds().toFloat(); auto c = b.getCentre();
        float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.47f, inner = R * 0.62f;
        float d = p.getDistanceFrom (c);
        if (d < inner - 16) return -2;
        if (d < inner || d > R) return -1;
        float ang = std::atan2 (p.x - c.x, -(p.y - c.y)); if (ang < 0) ang += juce::MathConstants<float>::twoPi;
        return ((int) std::floor ((ang / juce::MathConstants<float>::twoPi) * 12.0f + 0.5f)) % 12;
    }
    void set (const juce::String& id, float v) { if (auto* p = proc.apvts.getParameter (id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); } }
    void parameterChanged (const juce::String&, float) override {}
    void timerCallback() override { t += 1.0f / 30.0f; repaint(); }
    SanctuaryProcessor& proc; const lumen::Theme& theme; float t = 0; int hoverIdx = -1;
};

class SanctuaryEditor : public juce::AudioProcessorEditor
{
public:
    explicit SanctuaryEditor (SanctuaryProcessor&);
    ~SanctuaryEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    SanctuaryProcessor& proc;
    lumen::Theme theme = lumen::Theme::withAccent (juce::Colour (0xffc9a7ff));
    lumen::LookAndFeel lnf { theme };
    lumen::Backdrop backdrop { theme, &proc.energy };
    lumen::Header header { theme, "SANCTUARY", "Live Worship Pad Engine" };
    KeyWheel wheel { proc, theme };
    lumen::Panel soundPanel { theme, "Sound" }, livePanel { theme, "Live" }, spacePanel { theme, "Space & Output" };
    lumen::Choice mode { proc.apvts, ID::mode, "Mode" }, style { proc.apvts, ID::style, "Style" }, motionBars { proc.apvts, ID::motionBars, "Motion cycle" };
    lumen::Knob swell { proc.apvts, ID::swell, "Swell" }, air { proc.apvts, ID::air, "Air" }, depth { proc.apvts, ID::depth, "Depth" }, colour { proc.apvts, ID::colour, "Colour" },
                motion { proc.apvts, ID::motion, "Motion" }, transition { proc.apvts, ID::transition, "Transition" }, fade { proc.apvts, ID::fade, "Fade" },
                reverb { proc.apvts, ID::reverb, "Space" }, width { proc.apvts, ID::width, "Width" }, output { proc.apvts, ID::output, "Output", true };
    lumen::Toggle midiKeys { proc.apvts, ID::midiKeys, "MIDI key switch" }, modWheel { proc.apvts, ID::modWheel, "Mod wheel = Swell" };
    lumen::Meter meter { theme, proc.meterL, proc.meterR };
    juce::Label midiHelp;
    lumen::ABState ab;
};
