#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using P = AudioParameterFloat;
static const StringArray kWaves { "Saw", "Square", "Triangle", "Sine" };

AudioProcessorValueTreeState::ParameterLayout McGeeProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&] (const String& id, const String& name, float lo, float hi, float def, float skew = 1.0f, const String& unit = {})
    { l.add (std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.001f, skew), def, AudioParameterFloatAttributes().withLabel (unit))); };
    auto c = [&] (const String& id, const String& name, const StringArray& ch, int def) { l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, ch, def)); };

    c (ID::osc1Wave, "Osc 1 Wave", kWaves, 0); c (ID::osc2Wave, "Osc 2 Wave", kWaves, 0); c (ID::osc3Wave, "Osc 3 Wave", kWaves, 2);
    f (ID::osc1Lvl, "Osc 1 Level", 0, 1, 1); f (ID::osc2Lvl, "Osc 2 Level", 0, 1, 0.8f); f (ID::osc3Lvl, "Osc 3 Level", 0, 1, 0.5f);
    c (ID::osc2Oct, "Osc 2 Octave", { "-2", "-1", "0", "+1", "+2" }, 2); c (ID::osc3Oct, "Osc 3 Octave", { "-2", "-1", "0", "+1", "+2" }, 1);
    f (ID::osc2Det, "Osc 2 Detune", -50, 50, 7, 1, "ct"); f (ID::osc3Det, "Osc 3 Detune", -50, 50, -5, 1, "ct");
    f (ID::pw, "Pulse Width", 0.05f, 0.95f, 0.5f); f (ID::fat, "Fat", 0, 1, 0.3f);
    f (ID::cutoff, "Cutoff", 20, 18000, 1800, 0.3f, "Hz"); f (ID::reso, "Resonance", 0, 1, 0.3f); f (ID::envAmt, "Env Amount", -1, 1, 0.5f);
    f (ID::keyTrack, "Key Track", 0, 1, 0.5f); f (ID::fDrive, "Filter Drive", 0, 1, 0.2f);
    f (ID::fA, "F Attack", 0.001f, 5, 0.01f, 0.4f, "s"); f (ID::fD, "F Decay", 0.01f, 8, 0.4f, 0.4f, "s"); f (ID::fS, "F Sustain", 0, 1, 0.3f); f (ID::fR, "F Release", 0.01f, 10, 0.5f, 0.4f, "s");
    f (ID::aA, "A Attack", 0.001f, 5, 0.005f, 0.4f, "s"); f (ID::aD, "A Decay", 0.01f, 8, 0.3f, 0.4f, "s"); f (ID::aS, "A Sustain", 0, 1, 0.8f); f (ID::aR, "A Release", 0.01f, 10, 0.4f, 0.4f, "s");
    f (ID::lfoRate, "LFO Rate", 0.05f, 20, 4, 0.5f, "Hz"); f (ID::lfoPitch, "LFO > Pitch", 0, 1, 0); f (ID::lfoCutoff, "LFO > Cutoff", 0, 1, 0);
    f (ID::glide, "Glide", 0, 1, 0, 0.5f, "s"); l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::mono, 1 }, "Mono", false));
    f (ID::drive, "Drive", 0, 1, 0); f (ID::chorus, "Chorus", 0, 1, 0);
    f (ID::delayTime, "Delay Time", 0.02f, 1.5f, 0.375f, 0.6f, "s"); f (ID::delayFb, "Delay Feedback", 0, 0.9f, 0.35f); f (ID::delayMix, "Delay Mix", 0, 1, 0);
    f (ID::volume, "Volume", -36, 6, -6, 1, "dB");
    return l;
}

std::vector<lumen::Preset> McGeeProcessor::factoryPresets()
{
    using M = std::map<String, float>;
    return {
        { "Init Fat Saw",    M { { ID::osc1Wave, 0 }, { ID::osc2Wave, 0 }, { ID::osc3Wave, 2 }, { ID::fat, 0.3f }, { ID::cutoff, 1800 }, { ID::reso, 0.3f }, { ID::envAmt, 0.5f }, { ID::chorus, 0 }, { ID::delayMix, 0 }, { ID::mono, 0 }, { ID::glide, 0 } } },
        { "Model Bass",      M { { ID::osc1Wave, 0 }, { ID::osc2Wave, 1 }, { ID::osc3Wave, 0 }, { ID::osc2Oct, 2 }, { ID::osc3Oct, 1 }, { ID::osc2Det, 3 }, { ID::fat, 0.1f }, { ID::cutoff, 400 }, { ID::reso, 0.45f }, { ID::envAmt, 0.7f }, { ID::fD, 0.25f }, { ID::fS, 0.1f }, { ID::aR, 0.2f }, { ID::fDrive, 0.5f }, { ID::mono, 1 }, { ID::glide, 0.08f }, { ID::chorus, 0 }, { ID::delayMix, 0 } } },
        { "Warm Worship Pad", M { { ID::osc1Wave, 0 }, { ID::osc2Wave, 0 }, { ID::osc3Wave, 2 }, { ID::osc2Det, 12 }, { ID::osc3Det, -9 }, { ID::fat, 0.8f }, { ID::cutoff, 900 }, { ID::reso, 0.15f }, { ID::envAmt, 0.3f }, { ID::fA, 1.2f }, { ID::fD, 2.0f }, { ID::fS, 0.6f }, { ID::aA, 0.9f }, { ID::aR, 2.5f }, { ID::fR, 2.5f }, { ID::chorus, 0.6f }, { ID::delayMix, 0.25f }, { ID::delayTime, 0.5f }, { ID::mono, 0 }, { ID::glide, 0 } } },
        { "Brass Lead",      M { { ID::osc1Wave, 0 }, { ID::osc2Wave, 0 }, { ID::osc3Wave, 0 }, { ID::osc2Det, 5 }, { ID::osc3Det, -4 }, { ID::osc3Oct, 2 }, { ID::fat, 0.2f }, { ID::cutoff, 1200 }, { ID::reso, 0.2f }, { ID::envAmt, 0.8f }, { ID::fA, 0.05f }, { ID::fD, 0.4f }, { ID::fS, 0.5f }, { ID::aA, 0.03f }, { ID::mono, 1 }, { ID::glide, 0.05f }, { ID::fDrive, 0.4f }, { ID::chorus, 0 }, { ID::delayMix, 0.15f } } },
        { "Glass Keys",      M { { ID::osc1Wave, 2 }, { ID::osc2Wave, 3 }, { ID::osc3Wave, 3 }, { ID::osc2Oct, 3 }, { ID::osc3Oct, 4 }, { ID::osc2Lvl, 0.5f }, { ID::osc3Lvl, 0.25f }, { ID::fat, 0 }, { ID::cutoff, 6000 }, { ID::reso, 0.1f }, { ID::envAmt, 0.6f }, { ID::fD, 1.2f }, { ID::fS, 0.0f }, { ID::aD, 1.5f }, { ID::aS, 0.3f }, { ID::aR, 1.2f }, { ID::chorus, 0.5f }, { ID::delayMix, 0.3f }, { ID::mono, 0 }, { ID::glide, 0 } } },
        { "Squelch Acid",    M { { ID::osc1Wave, 1 }, { ID::osc2Wave, 1 }, { ID::osc3Wave, 0 }, { ID::pw, 0.3f }, { ID::fat, 0 }, { ID::cutoff, 300 }, { ID::reso, 0.85f }, { ID::envAmt, 0.9f }, { ID::fD, 0.18f }, { ID::fS, 0.05f }, { ID::mono, 1 }, { ID::glide, 0.12f }, { ID::fDrive, 0.6f }, { ID::chorus, 0 }, { ID::delayMix, 0.2f } } },
        { "Soft Strings",    M { { ID::osc1Wave, 0 }, { ID::osc2Wave, 0 }, { ID::osc3Wave, 0 }, { ID::osc2Det, 9 }, { ID::osc3Det, -11 }, { ID::osc3Oct, 2 }, { ID::fat, 1.0f }, { ID::cutoff, 2200 }, { ID::reso, 0.0f }, { ID::envAmt, 0.2f }, { ID::fA, 0.6f }, { ID::aA, 0.5f }, { ID::aR, 1.5f }, { ID::fR, 1.5f }, { ID::lfoRate, 5.5f }, { ID::lfoPitch, 0.08f }, { ID::chorus, 0.7f }, { ID::delayMix, 0 }, { ID::mono, 0 }, { ID::glide, 0 } } },
        { "Pluck Arp",       M { { ID::osc1Wave, 0 }, { ID::osc2Wave, 1 }, { ID::osc3Wave, 2 }, { ID::osc2Oct, 3 }, { ID::fat, 0.15f }, { ID::cutoff, 700 }, { ID::reso, 0.35f }, { ID::envAmt, 0.9f }, { ID::fD, 0.22f }, { ID::fS, 0 }, { ID::aD, 0.4f }, { ID::aS, 0 }, { ID::aR, 0.3f }, { ID::delayMix, 0.4f }, { ID::delayTime, 0.375f }, { ID::delayFb, 0.45f }, { ID::chorus, 0.2f }, { ID::mono, 0 }, { ID::glide, 0 } } },
    };
}

McGeeProcessor::McGeeProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MCGEE", createLayout()), presets (apvts, "McGEE", factoryPresets()) {}

void McGeeProcessor::prepareToPlay (double sr, int block) { engine.prepare (sr, block); vol.reset (sr, 0.02); }

void McGeeProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    auto g = [this] (const String& id) { return apvts.getRawParameterValue (id)->load(); };
    mcgee::SynthParams p;
    p.osc1Wave = (int) g (ID::osc1Wave); p.osc2Wave = (int) g (ID::osc2Wave); p.osc3Wave = (int) g (ID::osc3Wave);
    p.osc1Lvl = g (ID::osc1Lvl); p.osc2Lvl = g (ID::osc2Lvl); p.osc3Lvl = g (ID::osc3Lvl);
    p.osc2Oct = (int) g (ID::osc2Oct) - 2; p.osc3Oct = (int) g (ID::osc3Oct) - 2;
    p.osc2Detune = g (ID::osc2Det); p.osc3Detune = g (ID::osc3Det); p.pw = g (ID::pw); p.fat = g (ID::fat);
    p.cutoff = g (ID::cutoff); p.reso = g (ID::reso); p.envAmt = g (ID::envAmt); p.keyTrack = g (ID::keyTrack); p.filterDrive = g (ID::fDrive);
    p.fA = g (ID::fA); p.fD = g (ID::fD); p.fS = g (ID::fS); p.fR = g (ID::fR); p.aA = g (ID::aA); p.aD = g (ID::aD); p.aS = g (ID::aS); p.aR = g (ID::aR);
    p.lfoRate = g (ID::lfoRate); p.lfoToPitch = g (ID::lfoPitch); p.lfoToCutoff = g (ID::lfoCutoff); p.glide = g (ID::glide); p.mono = g (ID::mono) > 0.5f; p.drive = g (ID::drive);
    engine.setParams (p, g (ID::chorus), g (ID::delayTime), g (ID::delayFb), g (ID::delayMix));

    buffer.clear();
    float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);
    int pos = 0;
    for (const auto meta : midi)
    {
        int at = jlimit (0, n, meta.samplePosition);
        if (at > pos) { engine.render (L + pos, R + pos, at - pos); pos = at; }
        engine.handleMidi (meta.getMessage());
    }
    if (pos < n) engine.render (L + pos, R + pos, n - pos);

    vol.setTargetValue (Decibels::decibelsToGain (g (ID::volume)));
    for (int i = 0; i < n; ++i) { float v = vol.getNextValue(); L[i] *= v; R[i] *= v; }
    voiceCount.store (engine.activeVoices());
    lumen::dsp::trackPeak (meterL, L, n); lumen::dsp::trackPeak (meterR, R, n);
    energy.store (jlimit (0.0f, 1.0f, meterL.load() * 1.5f));
}

void McGeeProcessor::getStateInformation (MemoryBlock& dest) { if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest); }
void McGeeProcessor::setStateInformation (const void* data, int size) { if (auto xml = getXmlFromBinary (data, size)) apvts.replaceState (ValueTree::fromXml (*xml)); }
AudioProcessorEditor* McGeeProcessor::createEditor() { return new McGeeEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new McGeeProcessor(); }
