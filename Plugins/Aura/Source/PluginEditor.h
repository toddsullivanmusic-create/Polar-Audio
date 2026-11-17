#pragma once
#include "PluginProcessor.h"

// Decay visualiser: a breathing ring whose size follows the tail energy, with an RT60 curve.
class AuraVisual : public juce::Component, private juce::Timer
{
public:
    AuraVisual (AuraProcessor& p, const polar::Theme& t) : proc (p), theme (t) { setInterceptsMouseClicks (false, false); startTimerHz (30); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        auto c = b.getCentre();
        float maxR = juce::jmin (b.getWidth(), b.getHeight()) * 0.42f;
        float size  = proc.apvts.getRawParameterValue (ID::size)->load() * 0.01f;
        float decay = proc.apvts.getRawParameterValue (ID::decay)->load();
        // outer rings = room size
        for (int i = 0; i < 4; ++i)
        {
            float r = maxR * (0.35f + 0.65f * size) * (0.55f + 0.15f * i);
            g.setColour (theme.accent.withAlpha (0.08f + 0.04f * (3 - i)));
            g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
        }
        // energy bloom
        float e = juce::jlimit (0.0f, 1.0f, smooth * 2.5f);
        float r = maxR * (0.25f + 0.75f * e);
        juce::ColourGradient gr (theme.accent.withAlpha (0.55f * e + 0.08f), c.x, c.y, juce::Colours::transparentBlack, c.x + r, c.y, true);
        g.setGradientFill (gr);
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        // decay curve
        juce::Path curve;
        auto area = b.reduced (18).removeFromBottom (b.getHeight() * 0.3f);
        for (int i = 0; i <= 60; ++i)
        {
            float t = (float) i / 60.0f;
            float y = std::exp (-6.9f * t * (10.0f / juce::jmax (0.1f, decay)) * 0.3f);
            auto pt = juce::Point<float> (area.getX() + area.getWidth() * t, area.getBottom() - area.getHeight() * y);
            i == 0 ? curve.startNewSubPath (pt) : curve.lineTo (pt);
        }
        g.setColour (theme.textFaint);
        g.strokePath (curve, juce::PathStrokeType (1.5f));
        g.setFont (polar::uiFont (10.5f));
        g.drawText (juce::String (decay, 2) + " s", area.toNearestInt().removeFromTop (14), juce::Justification::topRight);
    }
private:
    void timerCallback() override { float e = proc.tailEnergy.load(); smooth += (e - smooth) * (e > smooth ? 0.5f : 0.08f); repaint(); }
    AuraProcessor& proc; const polar::Theme& theme; float smooth = 0;
};

class AuraEditor : public juce::AudioProcessorEditor
{
public:
    explicit AuraEditor (AuraProcessor&);
    ~AuraEditor() override;
    void resized() override;
private:
    AuraProcessor& proc;
    polar::Theme theme = polar::Theme::withAccent (juce::Colour (0xff5b8cff));
    polar::LookAndFeel lnf { theme };
    polar::Backdrop backdrop { theme, &proc.tailEnergy };
    polar::Header header { theme, "AURA", "Algorithmic Reverb" };
    polar::Panel spacePanel { theme, "Space" }, tonePanel { theme, "Tone" }, motionPanel { theme, "Motion" }, outPanel { theme, "Output" };
    polar::Knob size { proc.apvts, ID::size, "Size" }, decay { proc.apvts, ID::decay, "Decay" }, predelay { proc.apvts, ID::predelay, "Pre-delay" },
                diffusion { proc.apvts, ID::diffusion, "Diffusion" }, damp { proc.apvts, ID::damp, "Damping" }, lowcut { proc.apvts, ID::lowcut, "Low Cut" },
                modRate { proc.apvts, ID::modRate, "Rate" }, modDepth { proc.apvts, ID::modDepth, "Depth" }, shimmer { proc.apvts, ID::shimmer, "Shimmer" },
                width { proc.apvts, ID::width, "Width" }, mix { proc.apvts, ID::mix, "Mix" }, duck { proc.apvts, ID::duck, "Duck" }, output { proc.apvts, ID::output, "Output", true };
    polar::Choice colour { proc.apvts, ID::colour, "Colour circuit" };
    polar::Toggle freeze { proc.apvts, ID::freeze, "Freeze" };
    polar::Meter meter { theme, proc.meterL, proc.meterR };
    AuraVisual visual { proc, theme };
    polar::ABState ab;
};
