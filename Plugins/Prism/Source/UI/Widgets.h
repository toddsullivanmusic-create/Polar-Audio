#pragma once
#include "../PluginProcessor.h"
#include "PrismLook.h"

namespace prism::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// Use the parameter's own text ("62 %", "3.5 dB") under every knob
inline void useParamText (polar::Knob& k, APVTS& s, const juce::String& id, juce::Colour accent)
{
    if (auto* prm = s.getParameter (id))
    {
        k.slider.textFromValueFunction = [prm] (double v) { return prm->getText (prm->convertTo0to1 ((float) v), 0); };
        k.slider.updateText();
    }
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    // set the text-box colours on the slider itself, so they hold whichever LookAndFeel built the box
    k.slider.setColour (juce::Slider::textBoxTextColourId, Palette::ink2());
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxHighlightColourId, accent.withAlpha (0.25f));
    k.label.setColour (juce::Label::textColourId, Palette::ink());
    k.label.setFont (polar::uiFont (11.5f, true));
}

inline void setParam (APVTS& s, const juce::String& id, float v)
{
    if (auto* prm = s.getParameter (id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); prm->endChangeGesture(); }
}

// ------------------------------------------------------------------------------------------------
// Stomp footswitch with an LED above it
// ------------------------------------------------------------------------------------------------
class FootSwitch : public juce::Button
{
public:
    FootSwitch (juce::Colour led) : juce::Button ("foot"), ledColour (led) { setClickingTogglesState (true); }
    juce::Colour ledColour;
    juce::String caption;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto b = getLocalBounds().toFloat();
        const bool on = getToggleState();
        // LED
        auto led = juce::Rectangle<float> (b.getCentreX() - 5, b.getY() + 2, 10, 10);
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (ledColour.withAlpha (0.45f), led.getCentreX(), led.getCentreY(), ledColour.withAlpha (0.0f), led.getCentreX() + 14, led.getCentreY(), true));
            g.fillEllipse (led.expanded (12));
        }
        g.setColour (on ? ledColour : Palette::line().darker (0.1f)); g.fillEllipse (led);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.7f : 0.4f)); g.fillEllipse (led.reduced (3).translated (-1, -1));
        // switch
        const float r = juce::jmin (b.getWidth(), b.getHeight() - 18) * 0.5f - 2;
        auto c = juce::Point<float> (b.getCentreX(), b.getBottom() - r - 2);
        juce::Path outer; outer.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
        juce::DropShadow (juce::Colours::black.withAlpha (down ? 0.12f : 0.24f), down ? 4 : 9, { 0, down ? 1 : 4 }).drawForPath (g, outer);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffdfdfd), c.x, c.y - r, juce::Colour (0xffc9c7c2), c.x, c.y + r, false));
        g.fillPath (outer);
        const float ir = r * 0.72f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d6d1), c.x, c.y - ir, juce::Colour (0xfff9f9f8), c.x, c.y + ir, false));
        g.fillEllipse (c.x - ir, c.y - ir, ir * 2, ir * 2);
        g.setColour (juce::Colour (over ? 0x30000000 : 0x18000000)); g.drawEllipse (c.x - ir, c.y - ir, ir * 2, ir * 2, 1.0f);
        if (caption.isNotEmpty())
        {
            g.setColour (Palette::ink2()); g.setFont (polar::uiFont (9.5f, true));
            g.drawText (caption, juce::Rectangle<float> (c.x - ir, c.y - 6, ir * 2, 12), juce::Justification::centred);
        }
    }
};

// ------------------------------------------------------------------------------------------------
// Animated per-module screen
// ------------------------------------------------------------------------------------------------
class ModuleScreen : public juce::Component
{
public:
    ModuleScreen (PrismProcessor& p, int moduleIndex) : proc (p), m (moduleIndex) { setInterceptsMouseClicks (false, false); }

    void tick (float dt)
    {
        t += dt;
        const float e = proc.module (m)->energy.load();
        level += (juce::jlimit (0.0f, 1.0f, e * 4.0f) - level) * (e * 4.0f > level ? 0.5f : 0.08f);
        if (m == 2) { for (auto& d : dots) d.life -= dt; if (rnd.nextFloat() < 0.4f) spawnDot(); }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const auto accent = Palette::module (m);
        g.setColour (accent.withAlpha (0.07f)); g.fillRoundedRectangle (b, 12.0f);
        g.setColour (accent.withAlpha (0.18f)); g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);
        // dotted grid
        g.setColour (accent.withAlpha (0.12f));
        for (float x = b.getX() + 12; x < b.getRight() - 6; x += 12)
            for (float y = b.getY() + 12; y < b.getBottom() - 6; y += 12) g.fillEllipse (x - 0.8f, y - 0.8f, 1.6f, 1.6f);

        auto area = b.reduced (12, 10);
        const auto& s = proc.apvts;
        const int type = (int) s.getRawParameterValue (PID::m (m, PID::type))->load();
        const float amt = s.getRawParameterValue (PID::m (m, PID::amount))->load();
        const float tw = s.getRawParameterValue (PID::m (m, PID::tweak))->load();
        const bool on = s.getRawParameterValue (PID::m (m, PID::on))->load() > 0.5f;
        const float alpha = on ? 1.0f : 0.35f;
        g.saveState(); g.reduceClipRegion (b.toNearestInt());
        switch (m)
        {
            case 0: drawColor (g, area, type, amt, tw, accent.withMultipliedAlpha (alpha)); break;
            case 1: drawMotion (g, area, type, amt, tw, accent.withMultipliedAlpha (alpha)); break;
            case 2: drawSpace (g, area, type, amt, tw, accent.withMultipliedAlpha (alpha)); break;
            default: drawAir (g, area, type, amt, tw, accent.withMultipliedAlpha (alpha)); break;
        }
        g.restoreState();
        g.setColour (Palette::ink().withAlpha (0.55f * alpha + 0.2f)); g.setFont (polar::uiFont (10.5f, true));
        g.drawText (readout (type, amt, tw), b.reduced (10, 6).toNearestInt(), juce::Justification::topRight);
    }

private:
    juce::String readout (int type, float amt, float tw) const
    {
        if (m == 2)
        {
            const bool sync = proc.apvts.getRawParameterValue (PID::sync)->load() > 0.5f;
            if (type == SpaceModule::Grain) return juce::String (juce::roundToInt (3 + amt * 45)) + " grains/s";
            if (sync) { int n; auto* d = divisions (n); return juce::String (d[divisionIndex (tw)].name); }
            return juce::String (juce::roundToInt (SpaceModule::timeSeconds (tw, false, 120) * 1000.0f)) + " ms";
        }
        if (m == 1 && type != MotionModule::Detune) return juce::String (MotionModule::rateHz (tw), 2) + " Hz";
        if (m == 3 && type <= AirModule::Bloom) return juce::String (0.4f * std::pow (2.0f, amt * 5.6f) * (type == AirModule::Bloom ? 1.5f : 1.0f), 1) + " s";
        if (m == 0 && type == ColorModule::Squash) return "-" + juce::String (proc.module (0)->uiB.load() * 24.0f, 1) + " dB";
        return {};
    }

    void drawColor (juce::Graphics& g, juce::Rectangle<float> a, int type, float amt, float tw, juce::Colour c)
    {
        juce::Path p;
        if (type <= ColorModule::Fold)
        {
            const float amp = 0.35f + 0.65f * level;
            for (int i = 0; i <= 160; ++i)
            {
                const float x = (float) i / 160.0f;
                const float in = std::sin ((x * 2.0f + t * 0.35f) * juce::MathConstants<float>::twoPi) * amp;
                float y = ColorModule::shape (type, in, amt, tw);
                if (type == ColorModule::Fuzz) y *= 1.1f;
                const juce::Point<float> pt (a.getX() + a.getWidth() * x, a.getCentreY() - juce::jlimit (-1.2f, 1.2f, y) * a.getHeight() * 0.4f);
                i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
            }
        }
        else if (type == ColorModule::Sweep)
        {
            const float fc = juce::jmax (100.0f, proc.module (0)->uiA.load());
            const float q = 0.7f + proc.apvts.getRawParameterValue (PID::m (0, PID::tone))->load() * 11.0f;
            for (int i = 0; i <= 160; ++i)
            {
                const float x = (float) i / 160.0f, f = 20.0f * std::pow (1000.0f, x);
                const float r = f / fc, mag = 1.0f / std::sqrt ((1 - r * r) * (1 - r * r) + (r / q) * (r / q));
                const float db = juce::jlimit (-24.0f, 18.0f, juce::Decibels::gainToDecibels (mag));
                const juce::Point<float> pt (a.getX() + a.getWidth() * x, a.getCentreY() - db / 24.0f * a.getHeight() * 0.45f);
                i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
            }
        }
        else
        {
            // compressor transfer curve + gain-reduction bar
            const float thr = -amt * 42.0f, ratio = 2.0f + tw * 18.0f;
            for (int i = 0; i <= 100; ++i)
            {
                const float in = -60.0f + 60.0f * (float) i / 100.0f;
                const float out = in > thr ? thr + (in - thr) / ratio : in;
                const juce::Point<float> pt (a.getX() + a.getWidth() * 0.8f * (float) i / 100.0f, a.getBottom() - (out + 60.0f) / 60.0f * a.getHeight());
                i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
            }
            const float gr = juce::jlimit (0.0f, 1.0f, proc.module (0)->uiB.load() * 1.6f);
            auto bar = juce::Rectangle<float> (a.getRight() - 12, a.getY(), 8, a.getHeight());
            g.setColour (c.withAlpha (0.15f)); g.fillRoundedRectangle (bar, 4);
            g.setColour (c); g.fillRoundedRectangle (bar.withHeight (bar.getHeight() * gr), 4);
        }
        g.setColour (c.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawMotion (juce::Graphics& g, juce::Rectangle<float> a, int type, float amt, float tw, juce::Colour c)
    {
        const float ph = proc.module (1)->uiA.load();
        const float depth = 0.15f + 0.85f * amt;
        const float tone = proc.apvts.getRawParameterValue (PID::m (1, PID::tone))->load();
        auto wave = [&] (float x)
        {
            const float s = std::sin ((x * 2.0f + ph) * juce::MathConstants<float>::twoPi);
            if (type == MotionModule::Tremolo) { const float k = 1 + tone * 12; return std::tanh (k * s) / std::tanh (k); }
            if (type == MotionModule::Vibrato) return s * (1 - tone) + std::sin ((x * 7.3f + ph * 3.1f) * 6.28f) * 0.4f * tone;
            return s;
        };
        const int lines = type == MotionModule::Chorus ? 3 : (type == MotionModule::Detune ? 2 : 1);
        for (int v = 0; v < lines; ++v)
        {
            juce::Path p;
            for (int i = 0; i <= 140; ++i)
            {
                const float x = (float) i / 140.0f;
                float y = wave (x + (float) v / 3.0f) * depth;
                if (type == MotionModule::Detune) y = std::sin ((x * (3.0f + v * 0.25f) + t * 0.2f) * 6.28f) * depth * 0.8f;
                const juce::Point<float> pt (a.getX() + a.getWidth() * x, a.getCentreY() - y * a.getHeight() * 0.4f);
                i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
            }
            g.setColour (c.withAlpha (v == 0 ? 1.0f : 0.45f));
            g.strokePath (p, juce::PathStrokeType (v == 0 ? 2.2f : 1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        if (type == MotionModule::Phaser || type == MotionModule::Flanger)
        {
            const int notches = type == MotionModule::Phaser ? 3 : 6;
            for (int k = 0; k < notches; ++k)
            {
                const float x = std::fmod ((float) (k + 1) / (float) (notches + 1) * (0.7f + 0.3f * std::sin (ph * 6.28f)) + 0.05f, 1.0f);
                g.setColour (c.withAlpha (0.18f)); g.fillRect (a.getX() + a.getWidth() * x - 1.5f, a.getY(), 3.0f, a.getHeight());
            }
        }
        // the "now" dot
        const float y0 = wave (0.25f - ph * 0.5f) * depth;
        juce::ignoreUnused (y0, tw);
        const float dx = a.getX() + a.getWidth() * 0.5f, dy = a.getCentreY() - wave (0.5f) * depth * a.getHeight() * 0.4f;
        g.setColour (c.withAlpha (0.25f)); g.fillEllipse (dx - 9, dy - 9, 18, 18);
        g.setColour (c); g.fillEllipse (dx - 4.5f, dy - 4.5f, 9, 9);
    }

    void drawSpace (juce::Graphics& g, juce::Rectangle<float> a, int type, float amt, float tw, juce::Colour c)
    {
        if (type == SpaceModule::Grain)
        {
            for (auto& d : dots)
                if (d.life > 0)
                {
                    const float al = juce::jlimit (0.0f, 1.0f, d.life) * (0.3f + 0.7f * level);
                    g.setColour (c.withAlpha (al)); const float r = d.r * (1.0f + (1.0f - d.life));
                    g.fillEllipse (a.getX() + d.x * a.getWidth() - r, a.getY() + d.y * a.getHeight() - r, r * 2, r * 2);
                }
            return;
        }
        const bool sync = proc.apvts.getRawParameterValue (PID::sync)->load() > 0.5f;
        const float sec = juce::jmax (0.03f, SpaceModule::timeSeconds (tw, sync, proc.hostBpm.load()));
        const float span = 2.5f;
        const float fb = 0.45f + amt * 0.5f;   // display decay (readable even at low feedback)
        float h = 1.0f;
        const bool rev = type == SpaceModule::Reverse;
        const float pulse = std::fmod (t / sec, 1.0f);
        g.setColour (c); g.fillRoundedRectangle (a.getX(), a.getBottom() - a.getHeight() * 0.95f, 4, a.getHeight() * 0.95f, 2);
        for (int k = 1; k < 40 && k * sec < span; ++k)
        {
            h *= fb;
            const float x = a.getX() + a.getWidth() * (float) k * sec / span;
            const float bh = a.getHeight() * 0.9f * juce::jmax (0.04f, h) * (type == SpaceModule::Multi && (k % 4) ? 0.6f : 1.0f);
            const bool hot = (k - 1) == (int) (t / sec) % 12;
            g.setColour (c.withAlpha (hot ? 0.95f : 0.55f));
            if (type == SpaceModule::Ping) { const bool up = k % 2; g.fillRoundedRectangle (x - 2, up ? a.getCentreY() - bh * 0.5f : a.getCentreY(), 4, bh * 0.5f, 2); }
            else if (rev)
            {
                juce::Path tri; tri.addTriangle (x - 6, a.getBottom(), x + 2, a.getBottom(), x + 2, a.getBottom() - bh);
                g.fillPath (tri);
            }
            else g.fillRoundedRectangle (x - 2, a.getBottom() - bh, 4, bh, 2);
        }
        if (type == SpaceModule::Shimmer)
            for (int k = 0; k < 6; ++k)
            {
                const float x = a.getX() + std::fmod (t * 0.15f + k * 0.17f, 1.0f) * a.getWidth();
                const float y = a.getBottom() - std::fmod (t * 0.3f + k * 0.29f, 1.0f) * a.getHeight();
                g.setColour (c.withAlpha (0.5f * level + 0.15f)); g.fillEllipse (x - 2, y - 2, 4, 4);
            }
        juce::ignoreUnused (pulse);
    }

    void drawAir (juce::Graphics& g, juce::Rectangle<float> a, int type, float amt, float tw, juce::Colour c)
    {
        const auto ctr = a.getCentre();
        if (type <= AirModule::Bloom)
        {
            const float maxR = a.getWidth() * (0.25f + 0.35f * (type == AirModule::Bloom ? 1.0f : tw));
            const int rings = 6;
            for (int k = 0; k < rings; ++k)
            {
                const float ph = std::fmod (t * (0.12f + 0.25f * (1.0f - amt)) + (float) k / rings, 1.0f);
                const float r = maxR * ph;
                g.setColour (c.withAlpha ((1.0f - ph) * (0.25f + 0.6f * level)));
                if (type == AirModule::Plate) g.drawRect (juce::Rectangle<float> (r * 2.2f, r * 1.2f).withCentre (ctr), 1.6f);
                else g.drawEllipse (ctr.x - r, ctr.y - r * 0.6f, r * 2, r * 1.2f, 1.6f);
            }
            g.setColour (c); g.fillEllipse (ctr.x - 4, ctr.y - 4, 8, 8);
        }
        else if (type == AirModule::Spring)
        {
            juce::Path p;
            const int coils = 22;
            for (int i = 0; i <= coils * 8; ++i)
            {
                const float x = (float) i / (float) (coils * 8);
                const float wob = std::sin (t * 9.0f + x * 20.0f) * level * 6.0f;
                const juce::Point<float> pt (a.getX() + a.getWidth() * x, ctr.y + std::sin (x * coils * 6.28f) * (a.getHeight() * 0.22f) + wob);
                i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
            }
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (1.8f));
        }
        else if (type == AirModule::Cassette)
        {
            const float rr = a.getHeight() * 0.34f;
            for (int k = 0; k < 2; ++k)
            {
                const auto rc = juce::Point<float> (a.getX() + a.getWidth() * (k ? 0.72f : 0.28f), ctr.y);
                g.setColour (c.withAlpha (0.2f)); g.fillEllipse (rc.x - rr, rc.y - rr, rr * 2, rr * 2);
                g.setColour (c); g.drawEllipse (rc.x - rr, rc.y - rr, rr * 2, rr * 2, 2.0f);
                const float wobble = 1.0f + 0.15f * amt * std::sin (t * 3.4f);
                for (int s = 0; s < 3; ++s)
                {
                    const float ang = t * 2.2f * wobble + (float) s * 2.094f;
                    g.drawLine ({ rc, rc.getPointOnCircumference (rr * 0.8f, ang) }, 2.0f);
                }
            }
            g.setColour (c.withAlpha (0.7f));
            g.drawLine (a.getX() + a.getWidth() * 0.28f, ctr.y + rr, a.getX() + a.getWidth() * 0.72f, ctr.y + rr, 2.0f);
        }
        else
        {
            const bool active = proc.module (3)->uiA.load() > 0.5f;
            const int cols = 8, rows = 3;
            const float cw = a.getWidth() / cols, ch = a.getHeight() / rows;
            for (int r = 0; r < rows; ++r)
                for (int col = 0; col < cols; ++col)
                {
                    const bool lit = active && ((col + r * 3 + (int) (t * 12)) % 5 == 0);
                    g.setColour (c.withAlpha (lit ? 0.95f : 0.15f + 0.1f * ((col + r) % 2)));
                    g.fillRoundedRectangle (a.getX() + col * cw + 2, a.getY() + r * ch + 2, cw - 4, ch - 4, 3);
                }
        }
    }

    void spawnDot()
    {
        for (auto& d : dots) if (d.life <= 0) { d = { rnd.nextFloat(), rnd.nextFloat(), 1.5f + rnd.nextFloat() * 3.0f, 0.6f + rnd.nextFloat() * 0.8f }; return; }
    }

    struct Dot { float x = 0, y = 0, r = 2, life = 0; };
    std::array<Dot, 40> dots;
    juce::Random rnd;
    PrismProcessor& proc; int m; float t = 0, level = 0;
};

// ------------------------------------------------------------------------------------------------
// One module "pedal section"
// ------------------------------------------------------------------------------------------------
class ModuleCard : public juce::Component
{
public:
    std::function<void (int module, int direction)> onMove;

    ModuleCard (PrismProcessor& p, int moduleIndex)
        : proc (p), m (moduleIndex), accent (Palette::module (moduleIndex)), screen (p, moduleIndex),
          amount (p.apvts, PID::m (moduleIndex, PID::amount), "Amount"),
          tweak (p.apvts, PID::m (moduleIndex, PID::tweak), "Tweak"),
          tone (p.apvts, PID::m (moduleIndex, PID::tone), "Tone"),
          mix (p.apvts, PID::m (moduleIndex, PID::mix), "Mix"),
          foot (Palette::module (moduleIndex))
    {
        addAndMakeVisible (screen);
        for (auto* k : { &amount, &tweak, &tone, &mix }) { addAndMakeVisible (k); useParamText (*k, p.apvts, k == &amount ? PID::m (m, PID::amount) : k == &tweak ? PID::m (m, PID::tweak) : k == &tone ? PID::m (m, PID::tone) : PID::m (m, PID::mix), accent); }
        amount.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
        auto* typeParam = dynamic_cast<juce::AudioParameterChoice*> (p.apvts.getParameter (PID::m (m, PID::type)));
        for (int i = 0; i < 6; ++i)
        {
            auto* b = types.add (new juce::TextButton (typeParam->choices[i]));
            b->setColour (juce::TextButton::buttonOnColourId, accent);
            b->setClickingTogglesState (false);
            b->onClick = [this, i] { selectType (i); };
            addAndMakeVisible (b);
        }
        footAttach = std::make_unique<APVTS::ButtonAttachment> (p.apvts, PID::m (m, PID::on), foot);
        addAndMakeVisible (foot);
        for (auto* b : { &left, &right, &lock })
        {
            b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (b);
        }
        left.setButtonText (juce::String::fromUTF8 ("\xe2\x80\xb9")); right.setButtonText (juce::String::fromUTF8 ("\xe2\x80\xba"));
        left.setTooltip ("Move earlier in the chain"); right.setTooltip ("Move later in the chain");
        left.onClick = [this] { if (onMove) onMove (m, -1); };
        right.onClick = [this] { if (onMove) onMove (m, +1); };
        lock.setButtonText ("LOCK"); lock.setClickingTogglesState (true);
        lock.setColour (juce::TextButton::buttonOnColourId, Palette::ink());
        lock.setTooltip ("Lock this module so Dice leaves it alone");
        lock.setToggleState ((bool) p.apvts.state.getProperty ("lock" + juce::String (m)), juce::dontSendNotification);
        lock.onClick = [this] { proc.apvts.state.setProperty ("lock" + juce::String (m), lock.getToggleState(), nullptr); };
        refresh();
    }

    bool isLocked() const { return lock.getToggleState(); }
    void setChainPosition (int pos) { chainPos = pos; left.setEnabled (pos > 0); right.setEnabled (pos < 3); repaint(); }

    void tick (float dt)
    {
        screen.tick (dt);
        refresh();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (2);
        juce::Path body; body.addRoundedRectangle (b, 20.0f);
        juce::DropShadow (juce::Colours::black.withAlpha (0.09f), 22, { 0, 8 }).drawForPath (g, body);
        g.setColour (Palette::body()); g.fillPath (body);
        g.setColour (Palette::line()); g.strokePath (body, juce::PathStrokeType (1.0f));
        // colour band
        auto band = b.removeFromTop (46);
        juce::Path bp; bp.addRoundedRectangle (band.getX(), band.getY(), band.getWidth(), band.getHeight(), 20.0f, 20.0f, true, true, false, false);
        g.setGradientFill (juce::ColourGradient (accent.brighter (0.12f), band.getX(), band.getY(), accent.darker (0.05f), band.getRight(), band.getBottom(), false));
        g.fillPath (bp);
        g.setColour (juce::Colours::white.withAlpha (0.85f)); g.setFont (polar::uiFont (11.0f, true));
        g.drawText ("0" + juce::String (chainPos + 1), band.withTrimmedLeft (16).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white); g.setFont (polar::uiFont (17.0f, true));
        static const char* names[4] = { "COLOR", "MOTION", "SPACE", "AIR" };
        g.drawText (names[m], band.withTrimmedLeft (40).toNearestInt(), juce::Justification::centredLeft);
        // screws
        for (auto pt : { juce::Point<float> (b.getX() + 14, b.getBottom() - 14), juce::Point<float> (b.getRight() - 14, b.getBottom() - 14) })
        {
            g.setColour (Palette::line()); g.fillEllipse (pt.x - 5, pt.y - 5, 10, 10);
            g.setColour (Palette::ink3()); g.drawLine (pt.x - 3, pt.y - 1.5f, pt.x + 3, pt.y + 1.5f, 1.2f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 0);
        auto band = r.removeFromTop (48).reduced (0, 10);
        lock.setBounds (band.removeFromRight (48));
        band.removeFromRight (4);
        right.setBounds (band.removeFromRight (28)); left.setBounds (band.removeFromRight (28));
        r.removeFromTop (8);
        screen.setBounds (r.removeFromTop (100));
        r.removeFromTop (10);
        auto pills = r.removeFromTop (60);
        const int pw = (pills.getWidth() - 12) / 3;
        for (int i = 0; i < 6; ++i)
            types[i]->setBounds (pills.getX() + (i % 3) * (pw + 6), pills.getY() + (i / 3) * 32, pw, 26);
        r.removeFromTop (6);
        amount.setBounds (r.removeFromTop (112).withSizeKeepingCentre (130, 112));
        auto row = r.removeFromTop (84);
        const int kw = row.getWidth() / 3;
        tweak.setBounds (row.removeFromLeft (kw)); tone.setBounds (row.removeFromLeft (kw)); mix.setBounds (row);
        foot.setBounds (r.removeFromTop (64).withSizeKeepingCentre (64, 64));
    }

private:
    void selectType (int i)
    {
        const int old = (int) proc.apvts.getRawParameterValue (PID::m (m, PID::type))->load();
        setParam (proc.apvts, PID::m (m, PID::type), (float) i);
        // wet-only <-> complete effects in AIR get a sensible mix when you switch between them
        if (m == 3 && AirModule::isCompleteEffect (i) != AirModule::isCompleteEffect (old))
            setParam (proc.apvts, PID::m (m, PID::mix), AirModule::isCompleteEffect (i) ? 1.0f : 0.28f);
    }

    void refresh()
    {
        const int type = (int) proc.apvts.getRawParameterValue (PID::m (m, PID::type))->load();
        for (int i = 0; i < types.size(); ++i) types[i]->setToggleState (i == type, juce::dontSendNotification);
        if (type != shownType)
        {
            shownType = type;
            static const char* labels[4][6][3] = {
                { { "Drive", "Bias", "Tone" }, { "Gain", "Gate", "Tone" }, { "Bits", "Rate", "Tone" }, { "Folds", "Symmetry", "Tone" }, { "Sense", "Freq", "Reso" }, { "Squeeze", "Ratio", "Release" } },
                { { "Depth", "Rate", "Tone" }, { "Depth", "Rate", "Warble" }, { "Depth", "Rate", "Feedback" }, { "Depth", "Rate", "Shape" }, { "Depth", "Rate", "Feedback" }, { "Detune", "Drift", "Tone" } },
                { { "Feedback", "Time", "Tone" }, { "Feedback", "Time", "Tone" }, { "Feedback", "Length", "Tone" }, { "Feedback", "Time", "Tone" }, { "Feedback", "Time", "Tone" }, { "Density", "Size", "Pitch" } },
                { { "Decay", "Size", "Tone" }, { "Decay", "Size", "Tone" }, { "Decay", "Pre-delay", "Shimmer" }, { "Decay", "Tension", "Tone" }, { "Wobble", "Age", "Tone" }, { "Chance", "Slice", "Pitch" } } };
            amount.label.setText (labels[m][type][0], juce::dontSendNotification);
            tweak.label.setText (labels[m][type][1], juce::dontSendNotification);
            tone.label.setText (labels[m][type][2], juce::dontSendNotification);
        }
    }

    PrismProcessor& proc; int m; juce::Colour accent;
    ModuleScreen screen;
    polar::Knob amount, tweak, tone, mix;
    juce::OwnedArray<juce::TextButton> types;
    FootSwitch foot; std::unique_ptr<APVTS::ButtonAttachment> footAttach;
    juce::TextButton left, right, lock;
    int chainPos = 0, shownType = -1;
};

// ------------------------------------------------------------------------------------------------
// XY performance pad with gesture trace
// ------------------------------------------------------------------------------------------------
class XYPad : public juce::Component
{
public:
    explicit XYPad (PrismProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1);
        // 4-corner colour field
        juce::ColourGradient top (Palette::module (0).withAlpha (0.18f), b.getX(), b.getY(), Palette::module (1).withAlpha (0.18f), b.getRight(), b.getY(), false);
        g.setColour (Palette::well()); g.fillRoundedRectangle (b, 16);
        g.setGradientFill (top); g.fillRoundedRectangle (b, 16);
        juce::ColourGradient bot (Palette::module (3).withAlpha (0.0f), b.getX(), b.getY(), Palette::module (2).withAlpha (0.22f), b.getRight(), b.getBottom(), false);
        g.setGradientFill (bot); g.fillRoundedRectangle (b, 16);
        g.setColour (Palette::line()); g.drawRoundedRectangle (b, 16, 1.0f);
        g.setColour (juce::Colour (0x10000000));
        for (int i = 1; i < 6; ++i)
        {
            g.drawLine (b.getX() + b.getWidth() * i / 6.0f, b.getY() + 6, b.getX() + b.getWidth() * i / 6.0f, b.getBottom() - 6);
            g.drawLine (b.getX() + 6, b.getY() + b.getHeight() * i / 6.0f, b.getRight() - 6, b.getY() + b.getHeight() * i / 6.0f);
        }
        g.setColour (Palette::ink3()); g.setFont (polar::uiFont (10.0f, true));
        g.drawText (juce::String::fromUTF8 ("TWEAK \xe2\x86\x92"), b.reduced (10, 6).toNearestInt(), juce::Justification::bottomRight);
        g.drawText (juce::String::fromUTF8 ("\xe2\x86\x91 AMOUNT"), b.reduced (10, 6).toNearestInt(), juce::Justification::topLeft);

        // recorded gesture path
        const auto st = proc.gestureState();
        if (st == PrismProcessor::GestureState::Playing || st == PrismProcessor::GestureState::Recording)
        {
            juce::Path path;
            for (int i = 0; i < PrismProcessor::kGesturePoints; i += 2)
            {
                const juce::Point<float> pt (b.getX() + proc.gestX[(size_t) i] * b.getWidth(), b.getBottom() - proc.gestY[(size_t) i] * b.getHeight());
                i == 0 ? path.startNewSubPath (pt) : path.lineTo (pt);
            }
            g.setColour (Palette::ink().withAlpha (0.18f)); g.strokePath (path, juce::PathStrokeType (2.0f));
        }
        // trail
        for (int i = 0; i < (int) trail.size(); ++i)
        {
            const auto& tp = trail[(size_t) i];
            const float a = (float) i / (float) trail.size();
            g.setColour (Palette::module (i % 4).withAlpha (a * 0.5f));
            g.fillEllipse (b.getX() + tp.x * b.getWidth() - 3, b.getBottom() - tp.y * b.getHeight() - 3, 6, 6);
        }
        // puck
        const float x = proc.effPadX.load(), y = proc.effPadY.load();
        const juce::Point<float> c (b.getX() + x * b.getWidth(), b.getBottom() - y * b.getHeight());
        juce::Path puck; puck.addEllipse (c.x - 13, c.y - 13, 26, 26);
        juce::DropShadow (juce::Colours::black.withAlpha (0.25f), 10, { 0, 3 }).drawForPath (g, puck);
        g.setColour (juce::Colours::white); g.fillPath (puck);
        juce::ColourGradient cg (Palette::module (0), c.x - 8, c.y - 8, Palette::module (2), c.x + 8, c.y + 8, false);
        cg.addColour (0.5, Palette::module (1));
        g.setGradientFill (cg); g.fillEllipse (c.x - 8, c.y - 8, 16, 16);
        if (st == PrismProcessor::GestureState::Recording)
        {
            g.setColour (juce::Colour (0xffff3b30).withAlpha (0.5f + 0.5f * std::sin (anim * 8.0f)));
            g.drawEllipse (c.x - 17, c.y - 17, 34, 34, 2.0f);
        }
    }

    void tick (float dt)
    {
        anim += dt;
        trail.push_back ({ proc.effPadX.load(), proc.effPadY.load() });
        if (trail.size() > 24) trail.erase (trail.begin());
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto id : { PID::padX, PID::padY }) if (auto* prm = proc.apvts.getParameter (id)) prm->beginChangeGesture();
        if (proc.gestureState() == PrismProcessor::GestureState::Playing) proc.playGesture (false);
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto b = getLocalBounds().toFloat().reduced (1);
        const float x = clamp01 ((e.position.x - b.getX()) / b.getWidth()), y = clamp01 ((b.getBottom() - e.position.y) / b.getHeight());
        if (auto* prm = proc.apvts.getParameter (PID::padX)) prm->setValueNotifyingHost (x);
        if (auto* prm = proc.apvts.getParameter (PID::padY)) prm->setValueNotifyingHost (y);
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        for (auto id : { PID::padX, PID::padY }) if (auto* prm = proc.apvts.getParameter (id)) prm->endChangeGesture();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        setParam (proc.apvts, PID::padX, 0.5f); setParam (proc.apvts, PID::padY, 0.5f);
    }

private:
    PrismProcessor& proc;
    std::vector<juce::Point<float>> trail;
    float anim = 0;
};

// ------------------------------------------------------------------------------------------------
// Output scope (rainbow trace) + stereo meter
// ------------------------------------------------------------------------------------------------
class Scope : public juce::Component
{
public:
    explicit Scope (PrismProcessor& p) : proc (p) {}
    void tick()
    {
        const float l = proc.meterL.exchange (0.0f), r = proc.meterR.exchange (0.0f);
        mL = l > mL ? l : mL * 0.86f; mR = r > mR ? r : mR * 0.86f;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (Palette::well()); g.fillRoundedRectangle (b, 14);
        g.setColour (Palette::line()); g.drawRoundedRectangle (b.reduced (0.5f), 14, 1.0f);
        auto meters = b.removeFromRight (34).reduced (8, 12);
        auto area = b.reduced (12, 10);
        g.setColour (Palette::line()); g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());
        juce::Path p;
        const int w = PrismProcessor::kScope, start = proc.scopeWrite.load();
        const int shown = 512;
        for (int i = 0; i < shown; ++i)
        {
            const float v = juce::jlimit (-1.0f, 1.0f, proc.scope[(size_t) ((start - shown + i + w) % w)]);
            const juce::Point<float> pt (area.getX() + area.getWidth() * (float) i / (float) (shown - 1), area.getCentreY() - v * area.getHeight() * 0.48f);
            i == 0 ? p.startNewSubPath (pt) : p.lineTo (pt);
        }
        juce::ColourGradient grad (Palette::module (0), area.getX(), 0, Palette::module (3), area.getRight(), 0, false);
        grad.addColour (0.33, Palette::module (1)); grad.addColour (0.66, Palette::module (2));
        g.setGradientFill (grad); g.strokePath (p, juce::PathStrokeType (1.8f));
        auto bar = [&] (juce::Rectangle<float> r, float v)
        {
            g.setColour (Palette::line()); g.fillRoundedRectangle (r, 3);
            const float db = juce::Decibels::gainToDecibels (v, -60.0f);
            const float f = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 63.0f);
            g.setColour (db > -1.0f ? juce::Colour (0xffff3b30) : Palette::module (2)); g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * f), 3);
        };
        bar (meters.removeFromLeft (7), mL); meters.removeFromLeft (4); bar (meters.removeFromLeft (7), mR);
    }
private:
    PrismProcessor& proc; float mL = 0, mR = 0;
};

// ------------------------------------------------------------------------------------------------
// Logo: a prism splitting a beam into the four module colours; ray length follows each module's energy
// ------------------------------------------------------------------------------------------------
class Logo : public juce::Component
{
public:
    explicit Logo (PrismProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void tick() { for (int i = 0; i < 4; ++i) { const float e = juce::jlimit (0.0f, 1.0f, proc.module (i)->energy.load() * 5.0f); lv[i] += (e - lv[i]) * (e > lv[i] ? 0.5f : 0.1f); } repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float h = b.getHeight();
        const juce::Point<float> apex (b.getX() + h * 0.75f, b.getY() + h * 0.12f);
        const juce::Point<float> bl (b.getX() + h * 0.22f, b.getBottom() - h * 0.12f), br (b.getX() + h * 1.28f, b.getBottom() - h * 0.12f);
        // incoming beam
        g.setColour (Palette::ink3()); g.drawLine (b.getX(), b.getCentreY() + h * 0.12f, b.getX() + h * 0.5f, b.getCentreY(), 2.0f);
        // rays
        for (int i = 0; i < 4; ++i)
        {
            const float y = b.getY() + h * (0.32f + 0.14f * (float) i);
            const float len = h * (0.5f + 1.1f * lv[i]);
            g.setColour (Palette::module (i)); g.drawLine (b.getX() + h * 1.0f, b.getCentreY(), b.getX() + h * 1.0f + len, y, 2.6f);
        }
        juce::Path tri; tri.addTriangle (apex, bl, br);
        juce::DropShadow (juce::Colours::black.withAlpha (0.15f), 8, { 0, 3 }).drawForPath (g, tri);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, apex.x, apex.y, juce::Colour (0xffe9e7e2), bl.x, bl.y, false));
        g.fillPath (tri);
        g.setColour (Palette::ink()); g.strokePath (tri, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered));
    }
private:
    PrismProcessor& proc; float lv[4] {};
};
} // namespace prism::ui
