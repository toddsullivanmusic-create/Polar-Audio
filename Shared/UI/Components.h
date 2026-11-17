#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

namespace lumen
{
using APVTS = juce::AudioProcessorValueTreeState;

// ---------- Knob: rotary + label + APVTS attachment ----------
class Knob : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramID, const juce::String& labelText, bool bipolar = false)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setScrollWheelEnabled (true);
        slider.setNumDecimalPlacesToDisplay (2);
        slider.setDoubleClickReturnValue (true, 0.0);
        if (bipolar) slider.getProperties().set ("bipolar", true);
        addAndMakeVisible (slider);
        label.setText (labelText, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (uiFont (12.0f, true));
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (label);
        attachment = std::make_unique<APVTS::SliderAttachment> (state, paramID, slider);
        if (auto* p = state.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        slider.setBounds (r);
    }

    void setDim (bool dim) { label.setAlpha (dim ? 0.5f : 1.0f); }

    juce::Slider slider;
    juce::Label  label;
private:
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------- Choice box ----------
class Choice : public juce::Component
{
public:
    Choice (APVTS& state, const juce::String& paramID, const juce::String& labelText)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
            box.addItemList (p->choices, 1);
        addAndMakeVisible (box);
        label.setText (labelText, juce::dontSendNotification);
        label.setFont (uiFont (11.5f, true));
        label.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (label);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, paramID, box);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (16));
        box.setBounds (r.removeFromTop (28));
    }
    juce::ComboBox box;
    juce::Label label;
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// ---------- Toggle ----------
class Toggle : public juce::Component
{
public:
    Toggle (APVTS& state, const juce::String& paramID, const juce::String& text)
    {
        button.setButtonText (text);
        addAndMakeVisible (button);
        attachment = std::make_unique<APVTS::ButtonAttachment> (state, paramID, button);
    }
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;
private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// ---------- Panel: glass card with title ----------
class Panel : public juce::Component
{
public:
    Panel (const Theme& t, juce::String title) : theme (t), titleText (std::move (title)) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme.bg1.withAlpha (0.85f));
        g.fillRoundedRectangle (r, theme.radius);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (r.reduced (0.5f), theme.radius, 1.0f);
        if (titleText.isNotEmpty())
        {
            g.setColour (theme.textFaint);
            g.setFont (uiFont (10.5f, true));
            g.drawText (titleText.toUpperCase(), r.reduced (14, 10).toNearestInt().removeFromTop (14),
                        juce::Justification::centredLeft);
        }
    }
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (12).withTrimmedTop (titleText.isNotEmpty() ? 18 : 0); }
    const Theme& theme;
    juce::String titleText;
};

// ---------- Header: product name, preset browser, A/B, bypass ----------
class Header : public juce::Component
{
public:
    Header (const Theme& t, juce::String name, juce::String tagline) : theme (t), product (std::move (name)), tag (std::move (tagline))
    {
        presetBox.setTextWhenNothingSelected ("Presets");
        addAndMakeVisible (presetBox);
        abButton.setButtonText ("A/B");
        abButton.setClickingTogglesState (true);
        addAndMakeVisible (abButton);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        // logo mark: three concentric rings in accent
        auto c = juce::Point<float> (28.0f, r.getCentreY());
        for (int i = 0; i < 3; ++i)
        {
            float rad = 6.0f + i * 5.0f;
            g.setColour (theme.accent.withAlpha (0.9f - i * 0.28f));
            g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.6f);
        }
        g.setColour (theme.text);
        g.setFont (uiFont (20.0f, true));
        g.drawText (product, 52, 0, 260, getHeight(), juce::Justification::centredLeft);
        g.setColour (theme.textFaint);
        g.setFont (uiFont (11.0f));
        auto nameW = uiFont (20.0f, true).getStringWidth (product);
        g.drawText (tag, 52 + nameW + 12, 0, 300, getHeight(), juce::Justification::centredLeft);
        g.setColour (theme.line);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        abButton.setBounds (r.removeFromRight (52));
        r.removeFromRight (8);
        presetBox.setBounds (r.removeFromRight (220));
    }
    const Theme& theme;
    juce::String product, tag;
    juce::ComboBox presetBox;
    juce::TextButton abButton;
};

// ---------- Stereo level meter ----------
class Meter : public juce::Component, private juce::Timer
{
public:
    Meter (const Theme& t, std::atomic<float>& l, std::atomic<float>& r) : theme (t), left (l), right (r) { startTimerHz (30); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        auto draw = [&] (juce::Rectangle<float> bar, float v)
        {
            g.setColour (theme.bg2);
            g.fillRoundedRectangle (bar, 2.0f);
            float db = juce::Decibels::gainToDecibels (v, -60.0f);
            float p = juce::jmap (db, -60.0f, 3.0f, 0.0f, 1.0f);
            auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, p));
            juce::ColourGradient gr (theme.accent, bar.getX(), bar.getBottom(), juce::Colour (0xffff5d5d), bar.getX(), bar.getY(), false);
            gr.addColour (0.85, theme.accent);
            g.setGradientFill (gr);
            g.fillRoundedRectangle (fill, 2.0f);
        };
        auto w = (b.getWidth() - 3.0f) * 0.5f;
        draw (b.removeFromLeft (w), smL);
        b.removeFromLeft (3.0f);
        draw (b, smR);
    }
private:
    void timerCallback() override
    {
        auto l = left.exchange (0.0f), r = right.exchange (0.0f);
        smL = l > smL ? l : smL * 0.85f;
        smR = r > smR ? r : smR * 0.85f;
        repaint();
    }
    const Theme& theme;
    std::atomic<float>& left; std::atomic<float>& right;
    float smL = 0, smR = 0;
};

// ---------- Backdrop: slow aurora / particle field that reacts to signal level (the "flair") ----------
class Backdrop : public juce::Component, private juce::Timer
{
public:
    Backdrop (const Theme& t, std::atomic<float>* energySource = nullptr) : theme (t), energy (energySource)
    {
        setInterceptsMouseClicks (false, false);
        juce::Random rnd (42);
        for (auto& p : particles)
        {
            p.x = rnd.nextFloat(); p.y = rnd.nextFloat();
            p.r = 1.0f + rnd.nextFloat() * 2.2f;
            p.vx = (rnd.nextFloat() - 0.5f) * 0.0006f;
            p.vy = (rnd.nextFloat() - 0.5f) * 0.0006f;
            p.phase = rnd.nextFloat() * 6.283f;
        }
        startTimerHz (30);
    }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (theme.bg0);
        g.fillAll();
        // aurora: two soft radial glows that drift
        auto glow = [&] (float cx, float cy, float rad, juce::Colour c)
        {
            juce::ColourGradient gr (c, cx, cy, juce::Colours::transparentBlack, cx + rad, cy, true);
            g.setGradientFill (gr);
            g.fillEllipse (cx - rad, cy - rad, rad * 2, rad * 2);
        };
        float e = juce::jlimit (0.0f, 1.0f, level);
        glow (b.getWidth() * (0.25f + 0.05f * std::sin (t * 0.31f)), b.getHeight() * (0.3f + 0.04f * std::cos (t * 0.23f)),
              b.getWidth() * (0.45f + e * 0.1f), theme.accent.withAlpha (0.10f + e * 0.10f));
        glow (b.getWidth() * (0.8f + 0.05f * std::cos (t * 0.19f)), b.getHeight() * (0.85f + 0.04f * std::sin (t * 0.27f)),
              b.getWidth() * 0.4f, theme.accent.withHue (theme.accent.getHue() + 0.08f).withAlpha (0.07f + e * 0.08f));
        for (auto& p : particles)
        {
            float tw = 0.5f + 0.5f * std::sin (t * 1.3f + p.phase);
            g.setColour (juce::Colours::white.withAlpha (0.05f + 0.18f * tw * (0.4f + e)));
            g.fillEllipse (p.x * b.getWidth() - p.r, p.y * b.getHeight() - p.r, p.r * 2, p.r * 2);
        }
    }
private:
    void timerCallback() override
    {
        t += 1.0f / 30.0f;
        float target = energy ? juce::jlimit (0.0f, 1.0f, energy->load() * 2.0f) : 0.0f;
        level += (target - level) * (target > level ? 0.35f : 0.06f);
        for (auto& p : particles)
        {
            p.x += p.vx * (1.0f + level * 4.0f); p.y += p.vy * (1.0f + level * 4.0f);
            if (p.x < 0) p.x += 1; if (p.x > 1) p.x -= 1; if (p.y < 0) p.y += 1; if (p.y > 1) p.y -= 1;
        }
        repaint();
    }
    struct P { float x, y, r, vx, vy, phase; };
    std::array<P, 48> particles;
    const Theme& theme;
    std::atomic<float>* energy;
    float t = 0, level = 0;
};

// ---------- Preset manager (factory presets stored as param maps + user presets on disk) ----------
struct Preset { juce::String name; std::map<juce::String, float> values; };

class PresetManager
{
public:
    PresetManager (APVTS& s, juce::String productName, std::vector<Preset> factory)
        : state (s), product (std::move (productName)), factoryPresets (std::move (factory)) {}

    juce::File userDir() const
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Lumen Audio").getChildFile (product).getChildFile ("Presets");
    }

    void populate (juce::ComboBox& box)
    {
        box.clear (juce::dontSendNotification);
        int id = 1;
        box.addSectionHeading ("Factory");
        for (auto& p : factoryPresets) box.addItem (p.name, id++);
        auto files = userDir().findChildFiles (juce::File::findFiles, false, "*.lumenpreset");
        if (! files.isEmpty())
        {
            box.addSeparator();
            box.addSectionHeading ("User");
            for (auto& f : files) box.addItem (f.getFileNameWithoutExtension(), id++);
        }
        box.addSeparator();
        box.addItem ("Save as...", 9999);
        box.onChange = [this, &box]
        {
            int sel = box.getSelectedId();
            if (sel == 9999) { saveDialog (box); return; }
            if (sel >= 1 && sel <= (int) factoryPresets.size()) apply (factoryPresets[(size_t) sel - 1]);
            else if (sel > (int) factoryPresets.size())
            {
                auto fs = userDir().findChildFiles (juce::File::findFiles, false, "*.lumenpreset");
                int idx = sel - 1 - (int) factoryPresets.size();
                if (idx >= 0 && idx < fs.size()) loadFile (fs[idx]);
            }
        };
    }

    void apply (const Preset& p)
    {
        for (auto& [id, v] : p.values)
            if (auto* param = state.getParameter (id))
                param->setValueNotifyingHost (param->convertTo0to1 (v));
    }

    void applyIndex (int i) { if (i >= 0 && i < (int) factoryPresets.size()) apply (factoryPresets[(size_t) i]); }

    void loadFile (const juce::File& f)
    {
        if (auto xml = juce::XmlDocument::parse (f))
            state.replaceState (juce::ValueTree::fromXml (*xml));
    }

    void saveDialog (juce::ComboBox& box)
    {
        auto* aw = new juce::AlertWindow ("Save preset", "Name", juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", "My preset");
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw, &box] (int r)
        {
            if (r == 1)
            {
                auto name = aw->getTextEditorContents ("name");
                userDir().createDirectory();
                if (auto xml = state.copyState().createXml())
                    xml->writeTo (userDir().getChildFile (name + ".lumenpreset"));
                populate (box);
            }
            delete aw;
        }), false);
    }

    APVTS& state;
    juce::String product;
    std::vector<Preset> factoryPresets;
};

// ---------- A/B snapshot ----------
struct ABState
{
    juce::ValueTree a, b; bool onB = false;
    void toggle (APVTS& s)
    {
        if (onB) { b = s.copyState(); if (a.isValid()) s.replaceState (a); }
        else     { a = s.copyState(); if (b.isValid()) s.replaceState (b); }
        onB = ! onB;
    }
};
} // namespace lumen
