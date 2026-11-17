#pragma once
#include "PluginProcessor.h"

// Oscilloscope-style waveform preview of the current oscillator mix (static render, cheap) + voice LEDs
class McGeeScope : public juce::Component, private juce::Timer
{
public:
    McGeeScope (McGeeProcessor& p, const lumen::Theme& t) : proc (p), theme (t) { setInterceptsMouseClicks (false, false); startTimerHz (20); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (theme.bg1.withAlpha (0.7f)); g.fillRoundedRectangle (b, theme.radius);
        auto area = b.reduced (14);
        auto gp = [&] (const juce::String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
        auto wave = [] (int w, float t, float pw) {
            switch (w) { case 0: return 2 * t - 1; case 1: return t < pw ? 1.0f : -1.0f; case 2: return 1 - 4 * std::abs (t - 0.5f); default: return std::sin (t * 6.2832f); } };
        juce::Path path;
        int w1 = (int) gp (ID::osc1Wave), w2 = (int) gp (ID::osc2Wave), w3 = (int) gp (ID::osc3Wave);
        float l1 = gp (ID::osc1Lvl), l2 = gp (ID::osc2Lvl), l3 = gp (ID::osc3Lvl), pw = gp (ID::pw);
        float o2 = std::pow (2.0f, gp (ID::osc2Oct) - 2), o3 = std::pow (2.0f, gp (ID::osc3Oct) - 2);
        for (int i = 0; i <= 200; ++i)
        {
            float t = (float) i / 200.0f * 2.0f;
            float y = wave (w1, std::fmod (t, 1.0f), pw) * l1 + wave (w2, std::fmod (t * o2, 1.0f), pw) * l2 + wave (w3, std::fmod (t * o3, 1.0f), pw) * l3;
            y /= juce::jmax (0.5f, l1 + l2 + l3);
            auto pt = juce::Point<float> (area.getX() + area.getWidth() * (float) i / 200.0f, area.getCentreY() - y * area.getHeight() * 0.4f);
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
    void timerCallback() override { repaint(); }
    McGeeProcessor& proc; const lumen::Theme& theme;
};

class McGeeEditor : public juce::AudioProcessorEditor
{
public:
    explicit McGeeEditor (McGeeProcessor&);
    ~McGeeEditor() override;
    void resized() override;
private:
    McGeeProcessor& proc;
    lumen::Theme theme = lumen::Theme::withAccent (juce::Colour (0xff34d399));
    lumen::LookAndFeel lnf { theme };
    lumen::Backdrop backdrop { theme, &proc.energy };
    lumen::Header header { theme, "McGEE", "Analog Polysynth" };
    lumen::Panel oscPanel { theme, "Oscillators" }, filterPanel { theme, "Ladder Filter" }, envPanel { theme, "Envelopes" }, modPanel { theme, "Modulation" }, fxPanel { theme, "Effects" };
    McGeeScope scope { proc, theme };
    lumen::Choice w1 { proc.apvts, ID::osc1Wave, "Osc 1" }, w2 { proc.apvts, ID::osc2Wave, "Osc 2" }, w3 { proc.apvts, ID::osc3Wave, "Osc 3" },
                  o2 { proc.apvts, ID::osc2Oct, "Oct" }, o3 { proc.apvts, ID::osc3Oct, "Oct" };
    lumen::Knob l1 { proc.apvts, ID::osc1Lvl, "Level" }, l2 { proc.apvts, ID::osc2Lvl, "Level" }, l3 { proc.apvts, ID::osc3Lvl, "Level" },
                d2 { proc.apvts, ID::osc2Det, "Detune", true }, d3 { proc.apvts, ID::osc3Det, "Detune", true }, pw { proc.apvts, ID::pw, "PW" }, fat { proc.apvts, ID::fat, "Fat" },
                cutoff { proc.apvts, ID::cutoff, "Cutoff" }, reso { proc.apvts, ID::reso, "Reso" }, envAmt { proc.apvts, ID::envAmt, "Env", true }, keyTrack { proc.apvts, ID::keyTrack, "Key" }, fDrive { proc.apvts, ID::fDrive, "Drive" },
                fA { proc.apvts, ID::fA, "A" }, fD { proc.apvts, ID::fD, "D" }, fS { proc.apvts, ID::fS, "S" }, fR { proc.apvts, ID::fR, "R" },
                aA { proc.apvts, ID::aA, "A" }, aD { proc.apvts, ID::aD, "D" }, aS { proc.apvts, ID::aS, "S" }, aR { proc.apvts, ID::aR, "R" },
                lfoRate { proc.apvts, ID::lfoRate, "Rate" }, lfoPitch { proc.apvts, ID::lfoPitch, "Pitch" }, lfoCutoff { proc.apvts, ID::lfoCutoff, "Cutoff" }, glide { proc.apvts, ID::glide, "Glide" },
                drive { proc.apvts, ID::drive, "Drive" }, chorus { proc.apvts, ID::chorus, "Chorus" }, delayTime { proc.apvts, ID::delayTime, "Time" }, delayFb { proc.apvts, ID::delayFb, "Feedback" }, delayMix { proc.apvts, ID::delayMix, "Delay" }, volume { proc.apvts, ID::volume, "Volume", true };
    lumen::Toggle mono { proc.apvts, ID::mono, "Mono" };
    lumen::Meter meter { theme, proc.meterL, proc.meterR };
    juce::MidiKeyboardComponent keyboard { proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };
    lumen::ABState ab;
};
