#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "SynthEngine.h"
#include "UI/Components.h"

namespace ID
{
static const juce::String osc1Wave = "osc1Wave", osc2Wave = "osc2Wave", osc3Wave = "osc3Wave", osc1Lvl = "osc1Lvl", osc2Lvl = "osc2Lvl", osc3Lvl = "osc3Lvl",
    osc2Oct = "osc2Oct", osc3Oct = "osc3Oct", osc2Det = "osc2Det", osc3Det = "osc3Det", pw = "pw", fat = "fat", fm = "fm", fmRatio = "fmRatio",
    cutoff = "cutoff", reso = "reso", envAmt = "envAmt", keyTrack = "keyTrack", fDrive = "fDrive",
    fA = "fA", fD = "fD", fS = "fS", fR = "fR", aA = "aA", aD = "aD", aS = "aS", aR = "aR",
    lfoRate = "lfoRate", lfoPitch = "lfoPitch", lfoCutoff = "lfoCutoff", glide = "glide", mono = "mono", tune = "tune",
    wobble = "wobble", lofi = "lofi", haze = "haze", room = "room", drive = "drive",
    delayTime = "delayTime", delayFb = "delayFb", delayMix = "delayMix", volume = "volume";
}

class McGeeProcessor : public juce::AudioProcessor
{
public:
    McGeeProcessor();
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "McGEE"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    polar::PresetManager presets;
    std::atomic<float> meterL { 0 }, meterR { 0 }, energy { 0 };
    std::atomic<int> voiceCount { 0 };
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static std::vector<polar::Preset> factoryPresets();
    mcgee::SynthEngine engine;
    juce::LinearSmoothedValue<float> vol;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (McGeeProcessor)
};
