#pragma once
#include "PluginProcessor.h"

// Oscillator-mix preview drawn as a slightly "chewed" scope: the trace itself warbles with Wobble
// and pixelates with Lo-fi, so the texture stage is visible before you hear it. Voice LEDs below.
class McGeeScope : public juce::Component, private juce::Timer
{
public:
    McGeeScope (McGeeProcessor& p, const polar::Theme& t) : proc (p), theme (t) { setInterceptsMouseClicks (false, false); startTimerHz (30); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (theme.bg1.withAlpha (0.7f)); g.fillRoundedRectangle (b, theme.radius);
        auto area = b.reduced (14);
        auto gp = [&] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
        auto wave = [] (int w, float t, float pw, float fm, float ratio) {
            switch (w) { case 0: return 2 * t - 1; case 1: return t < pw ? 1.0f : -1.0f; case 2: return 1 - 4 * std::abs (t - 0.5f);
                         case 3: return std::sin (t * 6.2832f); default: return std::sin (t * 6.2832f + fm * 6.0f * std::sin (t * ratio * 6.2832f)); } };
        int w1 = (int) gp (ID::osc1Wave), w2 = (int) gp (ID::osc2Wave), w3 = (int) gp (ID::osc3Wave);
        float l1 = gp (ID::osc1Lvl), l2 = gp (ID::osc2Lvl), l3 = gp (ID::osc3Lvl), pw = gp (ID::pw), fm = gp (ID::fm), ratio = gp (ID::fmRatio);
        float o2 = std::pow (2.0f, gp (ID::osc2Oct) - 2), o3 = std::pow (2.0f, gp (ID::osc3Oct) - 2);
        float wobble = gp (ID::wobble), lofi = gp (ID::lofi);
        int steps = (int) juce::jmap (lofi, 0.0f, 1.0f, 200.0f, 28.0f);
        float warp = 1.0f + 0.08f * wobble * std::sin (phase * 0.9f);
        juce::Path path;
        for (int i = 0; i <= steps; ++i)
        {
            float t = (float) i / (float) steps * 2.0f * warp;
            float y = wave (w1, std::fmod (t, 1.0f), pw, fm, ratio) * l1 + wave (w2, std::fmod (t * o2, 1.0f), pw, fm, ratio) * l2 + wave (w3, std::fmod (t * o3, 1.0f), pw, fm, ratio) * l3;
            y /= juce::jmax (0.5f, l1 + l2 + l3);
            if (lofi > 0.01f) y = std::round (y * (12.0f - 8.0f * lofi)) / (12.0f - 8.0f * lofi);
            auto pt = juce::Point<float> (area.getX() + area.getWidth() * (float) i / (float) steps, area.getCentreY() - y * area.getHeight() * 0.4f);
            i == 0 ? path.startNewSubPath (pt) : path.lineTo (pt);
        }
        g.setColour (theme.accent.withAlpha (0.3f)); g.strokePath (path, juce::PathStrokeType (5.0f));
        g.setColour (theme.accent); g.strokePath (path, juce::PathStrokeType (1.8f));
        int vc = proc.voiceCount.load();
        for (int i = 0; i < 16; ++i)
        {
            auto led = juce::Rectangle<float> (area.getX() + i * 12.0f, area.getBottom() - 6, 8, 4);
            g.setColour (i < vc ? theme.accent : theme.line); g.fillRoundedRectangle (led, 2.0f);
        }
    }
private:
    void timerCallback() override { phase += 0.2f; repaint(); }
    McGeeProcessor& proc; const polar::Theme& theme; float phase = 0;
};

class McGeeEditor : public juce::AudioProcessorEditor
{
public:
    explicit McGeeEditor (McGeeProcessor&);
    ~McGeeEditor() override;
    void resized() override;
private:
    McGeeProcessor& proc;
    polar::Theme theme = polar::Theme::withAccent (juce::Colour (0xff34d399));
    polar::LookAndFeel lnf { theme };
    polar::Backdrop backdrop { theme, &proc.energy };
    polar::Header header { theme, "McGEE", "Lo-fi Analog Polysynth" };
    polar::Panel oscPanel { theme, "Oscillators" }, filterPanel { theme, "Ladder Filter" }, envPanel { theme, "Envelopes: filter | amp" },
                 modPanel { theme, "Modulation" }, texturePanel { theme, "Texture" }, fxPanel { theme, "Tape Delay & Output" };
    McGeeScope scope { proc, theme };
    polar::Choice w1 { proc.apvts, ID::osc1Wave, "Osc 1" }, w2 { proc.apvts, ID::osc2Wave, "Osc 2" }, w3 { proc.apvts, ID::osc3Wave, "Osc 3" },
                  o2 { proc.apvts, ID::osc2Oct, "Oct" }, o3 { proc.apvts, ID::osc3Oct, "Oct" };
    polar::Knob l1 { proc.apvts, ID::osc1Lvl, "Level" }, l2 { proc.apvts, ID::osc2Lvl, "Level" }, l3 { proc.apvts, ID::osc3Lvl, "Level" },
                d2 { proc.apvts, ID::osc2Det, "Detune", true }, d3 { proc.apvts, ID::osc3Det, "Detune", true }, pw { proc.apvts, ID::pw, "PW" }, fat { proc.apvts, ID::fat, "Fat" },
                fm { proc.apvts, ID::fm, "FM" }, fmRatio { proc.apvts, ID::fmRatio, "Ratio" },
                cutoff { proc.apvts, ID::cutoff, "Cutoff" }, reso { proc.apvts, ID::reso, "Reso" }, envAmt { proc.apvts, ID::envAmt, "Env", true }, keyTrack { proc.apvts, ID::keyTrack, "Key" }, fDrive { proc.apvts, ID::fDrive, "Drive" },
                fA { proc.apvts, ID::fA, "A" }, fD { proc.apvts, ID::fD, "D" }, fS { proc.apvts, ID::fS, "S" }, fR { proc.apvts, ID::fR, "R" },
                aA { proc.apvts, ID::aA, "A" }, aD { proc.apvts, ID::aD, "D" }, aS { proc.apvts, ID::aS, "S" }, aR { proc.apvts, ID::aR, "R" },
                lfoRate { proc.apvts, ID::lfoRate, "Rate" }, lfoPitch { proc.apvts, ID::lfoPitch, "Pitch" }, lfoCutoff { proc.apvts, ID::lfoCutoff, "Cutoff" }, glide { proc.apvts, ID::glide, "Glide" }, tune { proc.apvts, ID::tune, "Tune", true },
                wobble { proc.apvts, ID::wobble, "Wobble" }, lofi { proc.apvts, ID::lofi, "Lo-fi" }, haze { proc.apvts, ID::haze, "Haze" }, room { proc.apvts, ID::room, "Room" },
                drive { proc.apvts, ID::drive, "Drive" }, delayTime { proc.apvts, ID::delayTime, "Time" }, delayFb { proc.apvts, ID::delayFb, "Feedback" }, delayMix { proc.apvts, ID::delayMix, "Delay" }, volume { proc.apvts, ID::volume, "Volume", true };
    polar::Toggle mono { proc.apvts, ID::mono, "Mono" };
    polar::Meter meter { theme, proc.meterL, proc.meterR };
    juce::MidiKeyboardComponent keyboard { proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    polar::ABState ab;
};
