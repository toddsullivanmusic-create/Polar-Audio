#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "ReverbEngine.h"
#include "UI/Components.h"

namespace ID
{
static const juce::String mix = "mix", size = "size", decay = "decay", predelay = "predelay", damp = "damp",
    lowcut = "lowcut", diffusion = "diffusion", modRate = "modRate", modDepth = "modDepth", shimmer = "shimmer",
    width = "width", colour = "colour", freeze = "freeze", duck = "duck", output = "output";
}

class AuraProcessor : public juce::AudioProcessor
{
public:
    AuraProcessor();
    ~AuraProcessor() override = default;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "AURA"; }
    bool acceptsMidi() const override { return false; }
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
    std::atomic<float> meterL { 0 }, meterR { 0 }, tailEnergy { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::vector<polar::Preset> factoryPresets();

    aura::ReverbEngine engine;
    juce::AudioBuffer<float> wet;
    juce::dsp::Gain<float> outGain;
    juce::LinearSmoothedValue<float> mixSm, widthSm;
    polar::dsp::OnePole duckEnv;
    float duckState = 0;
    double sr = 44100;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuraProcessor)
};
