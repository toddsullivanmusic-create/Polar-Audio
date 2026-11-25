#pragma once
#include "PluginProcessor.h"
#include "UI/Widgets.h"

class PrismEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PrismEditor (PrismProcessor&);
    ~PrismEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void moveModule (int module, int direction);
    void layoutCards();
    juce::Rectangle<int> cardArea (int pos) const;

    PrismProcessor& proc;
    prism::ui::PrismLook look;
    juce::TooltipWindow tooltips { this, 600 };

    prism::ui::Logo logo { proc };
    juce::ComboBox presetBox;
    juce::TextButton abButton { "A/B" }, diceButton { "Dice" }, undoButton { "Undo" }, setA { "Set A" }, setB { "Set B" };
    juce::Slider morph;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttach;
    polar::ABState ab;

    juce::OwnedArray<prism::ui::ModuleCard> cards;
    std::array<int, 4> shownOrder { -1, -1, -1, -1 };

    prism::ui::XYPad pad { proc };
    juce::TextButton recButton { "REC" }, playButton { "PLAY" }, clearButton { "CLEAR" };
    juce::ComboBox gestureLen;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> gestureLenAttach;
    prism::ui::ConsoleKnob padDepth { proc.apvts, PID::padDepth, "Pad Depth", prism::ui::Palette::module (1) };

    using CK = prism::ui::ConsoleKnob;
    CK input  { proc.apvts, PID::input,  "Input",  prism::ui::Palette::ink(), CK::Size::Small, true };
    CK tilt   { proc.apvts, PID::tilt,   "Tilt",   prism::ui::Palette::ink(), CK::Size::Small, true };
    CK width  { proc.apvts, PID::width,  "Width",  prism::ui::Palette::ink() };
    CK duck   { proc.apvts, PID::duck,   "Duck",   prism::ui::Palette::ink() };
    CK gmix   { proc.apvts, PID::gmix,   "Mix",    prism::ui::Palette::ink() };
    CK output { proc.apvts, PID::output, "Output", prism::ui::Palette::ink(), CK::Size::Small, true };
    juce::ToggleButton sync { "Tempo sync" }, safety { "Safety limiter" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach, safetyAttach, freezeAttach;
    prism::ui::FootSwitch freeze { prism::ui::Palette::ice() };
    prism::ui::Scope scope { proc };

    juce::Rectangle<int> deck, perform, master, outArea;
    juce::Image chassis, deckFace;
    float anim = 0;
    float flashA = 0, flashB = 0;
};
