#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PadEngine.h"
#include "UI/Components.h"

namespace ID
{
static const juce::String key = "key", mode = "mode", style = "style", play = "play", swell = "swell", air = "air", depth = "depth",
    colour = "colour", motion = "motion", motionBars = "motionBars", transition = "transition", fade = "fade", reverb = "reverb",
    width = "width", output = "output", midiKeys = "midiKeys", modWheel = "modWheel";
}

class SanctuaryProcessor : public juce::AudioProcessor
{
public:
    SanctuaryProcessor();
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SANCTUARY"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    polar::PresetManager presets;
    std::atomic<float> meterL { 0 }, meterR { 0 }, energy { 0 }, transition { 1 }, swellLevel { 0 };
    std::atomic<double> hostBpm { 120.0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::vector<polar::Preset> factoryPresets();
    sanctuary::PadEngine engine;
    juce::LinearSmoothedValue<float> out;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SanctuaryProcessor)
};
