#pragma once
#include "PluginProcessor.h"

// Transfer-curve scope: the live circuit curve with a glowing "bead" riding the input level.
class GlowScope : public juce::Component, private juce::Timer
{
public:
    GlowScope (GlowProcessor& p, const polar::Theme& t) : proc (p), theme (t) { setInterceptsMouseClicks (false, false); startTimerHz (30); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (16);
        g.setColour (theme.bg1.withAlpha (0.6f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), theme.radius);
        g.setColour (theme.line);
        g.drawLine (b.getX(), b.getCentreY(), b.getRight(), b.getCentreY());
        g.drawLine (b.getCentreX(), b.getY(), b.getCentreX(), b.getBottom());
        int circuit = (int) proc.apvts.getRawParameterValue (ID::circuit)->load();
        float bias = proc.apvts.getRawParameterValue (ID::bias)->load() * 0.01f;
        float drive = juce::Decibels::decibelsToGain (proc.apvts.getRawParameterValue (ID::drive)->load());
        juce::Path curve;
        auto f = [&] (float x) { return glow::SaturationEngine::shape (x * drive, circuit, bias) / juce::jmax (1.0f, std::pow (drive, 0.6f)); };
        for (int i = 0; i <= 120; ++i)
        {
            float x = -1.0f + 2.0f * (float) i / 120.0f;
            float y = juce::jlimit (-1.2f, 1.2f, f (x));
            auto pt = juce::Point<float> (b.getX() + (x + 1) * 0.5f * b.getWidth(), b.getCentreY() - y * b.getHeight() * 0.42f);
            i == 0 ? curve.startNewSubPath (pt) : curve.lineTo (pt);
        }
        g.setColour (theme.accent.withAlpha (0.25f));
        g.strokePath (curve, juce::PathStrokeType (6.0f));
        g.setColour (theme.accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f));
        float lv = juce::jlimit (0.0f, 1.0f, level);
        float by = juce::jlimit (-1.2f, 1.2f, f (lv));
        auto bead = juce::Point<float> (b.getX() + (lv + 1) * 0.5f * b.getWidth(), b.getCentreY() - by * b.getHeight() * 0.42f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (bead.x - 4, bead.y - 4, 8, 8);
        g.setColour (theme.accent.withAlpha (0.4f));
        g.fillEllipse (bead.x - 10, bead.y - 10, 20, 20);
        g.setColour (theme.textFaint); g.setFont (polar::uiFont (10.5f));
        g.drawText ("IN", b.toNearestInt().removeFromBottom (14), juce::Justification::centredRight);
        g.drawText ("OUT", b.toNearestInt().removeFromTop (14), juce::Justification::centredLeft);
    }
private:
    void timerCallback() override { float l = proc.inputLevel.exchange (0.0f); level += (l - level) * (l > level ? 0.6f : 0.12f); repaint(); }
    GlowProcessor& proc; const polar::Theme& theme; float level = 0;
};

class GlowEditor : public juce::AudioProcessorEditor
{
public:
    explicit GlowEditor (GlowProcessor&);
    ~GlowEditor() override;
    void resized() override;
private:
    GlowProcessor& proc;
    polar::Theme theme = polar::Theme::withAccent (juce::Colour (0xffff8a3d));
    polar::LookAndFeel lnf { theme };
    polar::Backdrop backdrop { theme, &proc.energy };
    polar::Header header { theme, "GLOW", "Analog Saturation" };
    polar::Panel circuitPanel { theme, "Circuit" }, shapePanel { theme, "Shape" }, outPanel { theme, "Output" };
    polar::Knob drive { proc.apvts, ID::drive, "Drive" }, bias { proc.apvts, ID::bias, "Bias" }, focus { proc.apvts, ID::focus, "Focus", true },
                dynamics { proc.apvts, ID::dynamics, "Dynamics" }, tone { proc.apvts, ID::tone, "Tone", true },
                mix { proc.apvts, ID::mix, "Mix" }, output { proc.apvts, ID::output, "Output", true };
    polar::Choice circuit { proc.apvts, ID::circuit, "Circuit" }, oversample { proc.apvts, ID::oversample, "Oversampling" };
    polar::Toggle autogain { proc.apvts, ID::autogain, "Auto gain" };
    polar::Meter meter { theme, proc.meterL, proc.meterR };
    GlowScope scope { proc, theme };
    polar::ABState ab;
};
