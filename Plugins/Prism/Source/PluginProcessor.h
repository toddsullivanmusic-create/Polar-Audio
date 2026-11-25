#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "DSP/ColorModule.h"
#include "DSP/MotionModule.h"
#include "DSP/SpaceModule.h"
#include "DSP/AirModule.h"
#include "UI/Components.h"

namespace PID
{
// per-module parameter IDs are  <module><Suffix>, e.g. "colorAmount"
static const char* const modules[4]  = { "color", "motion", "space", "air" };
static const juce::String type = "Type", amount = "Amount", tweak = "Tweak", tone = "Tone", mix = "Mix", on = "On";
static const juce::String input = "input", output = "output", gmix = "mix", tilt = "tilt", width = "width", duck = "duck",
    order = "order", padX = "padX", padY = "padY", padDepth = "padDepth", freeze = "freeze", morph = "morph",
    sync = "sync", gestureLen = "gestureLen", safety = "safety";
inline juce::String m (int module, const juce::String& suffix) { return juce::String (modules[module]) + suffix; }
}

class PrismProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    PrismProcessor();
    ~PrismProcessor() override { stopTimer(); }

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PRISM"; }
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

    // ---- chain order: 24 permutations of the 4 modules
    static const std::array<std::array<int, 4>, 24>& permutations();
    static int permutationIndex (const std::array<int, 4>& order);
    std::array<int, 4> currentOrder() const;

    // ---- scenes / morph
    void captureScene (int slot);
    bool hasScene (int slot) const { return sceneValid[(size_t) slot]; }

    // ---- gesture recorder (XY pad)
    enum class GestureState { Idle, Armed, Recording, Playing };
    void armGesture();
    void playGesture (bool play);
    void clearGesture();
    GestureState gestureState() const { return gState.load(); }
    float gesturePhase() const { return gPhase.load(); }
    std::atomic<float> effPadX { 0.5f }, effPadY { 0.5f };   // what the DSP is using right now
    static constexpr int kGesturePoints = 512;
    std::array<float, kGesturePoints> gestX {}, gestY {};

    // ---- dice
    void randomise (const std::array<bool, 4>& locks);

    juce::AudioProcessorValueTreeState apvts;
    polar::PresetManager presets;
    juce::UndoManager undo;

    // ---- UI feeds
    prism::Module* module (int i) { return mods[(size_t) i]; }
    std::atomic<float> meterL { 0 }, meterR { 0 }, inLevel { 0 };
    std::atomic<double> hostBpm { 120.0 };
    static constexpr int kScope = 1024;
    std::array<float, kScope> scope {};
    std::atomic<int> scopeWrite { 0 };

    static std::vector<polar::Preset> factoryPresets();

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override;
    float p (const juce::String& id) const { return apvts.getRawParameterValue (id)->load(); }

    prism::ColorModule color; prism::MotionModule motion; prism::SpaceModule space; prism::AirModule air;
    std::array<prism::Module*, 4> mods { &color, &motion, &space, &air };

    juce::dsp::IIR::Filter<float> lowShelf[2], highShelf[2];
    float lastTilt = -99.0f;
    prism::HDelay dryDelay[2];
    juce::AudioBuffer<float> dryBuf, duckBuf;
    prism::EnvFollower duckEnv;
    juce::LinearSmoothedValue<float> inG, outG, mixS, widthS;
    double sr = 44100.0;
    float latencySamples = 0;

    // host position for the gesture clock
    std::atomic<double> hostPpq { 0.0 };
    std::atomic<bool> hostPlaying { false };

    // scenes
    std::array<std::vector<float>, 2> scene;
    std::array<bool, 2> sceneValid { false, false };
    float lastMorph = -1.0f;

    // gesture
    std::atomic<GestureState> gState { GestureState::Idle };
    std::atomic<float> gPhase { 0.0f };
    double freePhase = 0.0, lastTick = 0.0;
    int lastRecIdx = -1; bool recWrapped = false; float recStartPhase = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrismProcessor)
};
