#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SaturationEngine.h"
#include "UI/Components.h"

namespace ID
{
static const juce::String drive = "drive", bias = "bias", focus = "focus", dynamics = "dynamics", circuit = "circuit",
    tone = "tone", mix = "mix", output = "output", autogain = "autogain", oversample = "oversample";
}

class GlowProcessor : public juce::AudioProcessor
{
public:
    GlowProcessor();
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "GLOW"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    lumen::PresetManager presets;
    std::atomic<float> meterL { 0 }, meterR { 0 }, inputLevel { 0 }, energy { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::vector<lumen::Preset> factoryPresets();

    glow::SaturationEngine engine;
    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
    juce::dsp::IIR::Filter<float> tiltLow[2], tiltHigh[2];
    juce::AudioBuffer<float> dryBuf;
    juce::LinearSmoothedValue<float> mixSm, outSm, agSm;
    float rmsIn = 0, rmsOut = 0;
    int currentOS = -1;
    double sr = 44100;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlowProcessor)
};
