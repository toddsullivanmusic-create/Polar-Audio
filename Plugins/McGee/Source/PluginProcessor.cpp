#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using P = AudioParameterFloat;
static const StringArray kWaves { "Saw", "Square", "Triangle", "Sine", "FM Keys" };

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
    f (ID::fm, "FM Amount", 0, 1, 0.3f); f (ID::fmRatio, "FM Ratio", 0.5f, 8.0f, 2.0f, 0.6f);
    f (ID::cutoff, "Cutoff", 20, 18000, 1800, 0.3f, "Hz"); f (ID::reso, "Resonance", 0, 1, 0.3f); f (ID::envAmt, "Env Amount", -1, 1, 0.5f);
    f (ID::keyTrack, "Key Track", 0, 1, 0.5f); f (ID::fDrive, "Filter Drive", 0, 1, 0.2f);
    f (ID::fA, "F Attack", 0.001f, 5, 0.01f, 0.4f, "s"); f (ID::fD, "F Decay", 0.01f, 8, 0.4f, 0.4f, "s"); f (ID::fS, "F Sustain", 0, 1, 0.3f); f (ID::fR, "F Release", 0.01f, 10, 0.5f, 0.4f, "s");
    f (ID::aA, "A Attack", 0.001f, 5, 0.005f, 0.4f, "s"); f (ID::aD, "A Decay", 0.01f, 8, 0.3f, 0.4f, "s"); f (ID::aS, "A Sustain", 0, 1, 0.8f); f (ID::aR, "A Release", 0.01f, 10, 0.4f, 0.4f, "s");
    f (ID::lfoRate, "LFO Rate", 0.05f, 20, 4, 0.5f, "Hz"); f (ID::lfoPitch, "LFO > Pitch", 0, 1, 0); f (ID::lfoCutoff, "LFO > Cutoff", 0, 1, 0);
    f (ID::glide, "Glide", 0, 1, 0, 0.5f, "s"); l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::mono, 1 }, "Mono", false));
    f (ID::tune, "Tune", -12, 12, 0, 1, "st");
    f (ID::wobble, "Wobble", 0, 1, 0.3f); f (ID::lofi, "Lo-fi", 0, 1, 0.3f); f (ID::haze, "Haze", 0, 1, 0.4f); f (ID::room, "Room", 0, 1, 0.2f);
    f (ID::drive, "Drive", 0, 1, 0);
    f (ID::delayTime, "Delay Time", 0.02f, 1.5f, 0.375f, 0.6f, "s"); f (ID::delayFb, "Delay Feedback", 0, 0.9f, 0.35f); f (ID::delayMix, "Delay Mix", 0, 1, 0);
    f (ID::volume, "Volume", -36, 6, -6, 1, "dB");
    return l;
}

// Factory presets are written around the "underwater" lo-fi palette: every preset sets the whole texture stage.
std::vector<polar::Preset> McGeeProcessor::factoryPresets()
{
    using M = std::map<String, float>;
    auto base = [] (M m) {
        M d { { ID::osc1Wave, 0 }, { ID::osc2Wave, 0 }, { ID::osc3Wave, 2 }, { ID::osc1Lvl, 1 }, { ID::osc2Lvl, 0.8f }, { ID::osc3Lvl, 0.5f },
              { ID::osc2Oct, 2 }, { ID::osc3Oct, 1 }, { ID::osc2Det, 7 }, { ID::osc3Det, -5 }, { ID::pw, 0.5f }, { ID::fat, 0.3f }, { ID::fm, 0.3f }, { ID::fmRatio, 2 },
              { ID::cutoff, 1800 }, { ID::reso, 0.3f }, { ID::envAmt, 0.5f }, { ID::keyTrack, 0.5f }, { ID::fDrive, 0.2f },
              { ID::fA, 0.01f }, { ID::fD, 0.4f }, { ID::fS, 0.3f }, { ID::fR, 0.5f }, { ID::aA, 0.005f }, { ID::aD, 0.3f }, { ID::aS, 0.8f }, { ID::aR, 0.4f },
              { ID::lfoRate, 4 }, { ID::lfoPitch, 0 }, { ID::lfoCutoff, 0 }, { ID::glide, 0 }, { ID::mono, 0 }, { ID::tune, 0 },
              { ID::wobble, 0.3f }, { ID::lofi, 0.3f }, { ID::haze, 0.4f }, { ID::room, 0.2f }, { ID::drive, 0 },
              { ID::delayTime, 0.375f }, { ID::delayFb, 0.35f }, { ID::delayMix, 0 }, { ID::volume, -6 } };
        for (auto& kv : m) d[kv.first] = kv.second; return d; };
    return {
        { "Underwater Juno",   base ({ { ID::osc2Det, 11 }, { ID::fat, 0.65f }, { ID::cutoff, 1400 }, { ID::reso, 0.15f }, { ID::envAmt, 0.25f }, { ID::fA, 0.3f }, { ID::aA, 0.12f }, { ID::aR, 1.2f }, { ID::fR, 1.2f }, { ID::wobble, 0.55f }, { ID::lofi, 0.45f }, { ID::haze, 0.7f }, { ID::room, 0.3f } }) },
        { "Warbled EP",        base ({ { ID::osc1Wave, 4 }, { ID::osc2Wave, 4 }, { ID::osc3Wave, 3 }, { ID::osc2Det, 4 }, { ID::osc3Lvl, 0.35f }, { ID::osc3Oct, 3 }, { ID::fat, 0.15f }, { ID::fm, 0.45f }, { ID::fmRatio, 1 }, { ID::cutoff, 3200 }, { ID::reso, 0.1f }, { ID::envAmt, 0.4f }, { ID::fD, 1.4f }, { ID::fS, 0.15f }, { ID::aD, 1.8f }, { ID::aS, 0.35f }, { ID::aR, 0.6f }, { ID::wobble, 0.6f }, { ID::lofi, 0.5f }, { ID::haze, 0.35f }, { ID::room, 0.35f }, { ID::delayMix, 0.18f } }) },
        { "Cassette Pad",      base ({ { ID::osc1Wave, 2 }, { ID::osc2Wave, 0 }, { ID::osc2Lvl, 0.55f }, { ID::osc2Det, 9 }, { ID::osc3Det, -8 }, { ID::fat, 0.8f }, { ID::cutoff, 1100 }, { ID::reso, 0.1f }, { ID::envAmt, 0.2f }, { ID::fA, 1.0f }, { ID::aA, 0.8f }, { ID::aR, 2.5f }, { ID::fR, 2.5f }, { ID::wobble, 0.45f }, { ID::lofi, 0.6f }, { ID::haze, 0.8f }, { ID::room, 0.4f } }) },
        { "12-bit Bell",       base ({ { ID::osc1Wave, 4 }, { ID::osc2Wave, 3 }, { ID::osc3Wave, 3 }, { ID::osc2Oct, 3 }, { ID::osc3Oct, 4 }, { ID::osc2Lvl, 0.4f }, { ID::osc3Lvl, 0.2f }, { ID::fat, 0 }, { ID::fm, 0.7f }, { ID::fmRatio, 3.5f }, { ID::cutoff, 6000 }, { ID::reso, 0 }, { ID::envAmt, 0.5f }, { ID::fD, 1.0f }, { ID::fS, 0 }, { ID::aD, 2.2f }, { ID::aS, 0.0f }, { ID::aR, 1.5f }, { ID::wobble, 0.35f }, { ID::lofi, 0.7f }, { ID::haze, 0.3f }, { ID::room, 0.5f }, { ID::delayMix, 0.25f } }) },
        { "Chewed Bass",       base ({ { ID::osc1Wave, 1 }, { ID::osc2Wave, 0 }, { ID::osc3Wave, 3 }, { ID::osc2Oct, 2 }, { ID::osc3Oct, 1 }, { ID::osc2Det, 3 }, { ID::pw, 0.35f }, { ID::fat, 0.1f }, { ID::cutoff, 420 }, { ID::reso, 0.4f }, { ID::envAmt, 0.6f }, { ID::fD, 0.25f }, { ID::fS, 0.1f }, { ID::aR, 0.2f }, { ID::fDrive, 0.5f }, { ID::mono, 1 }, { ID::glide, 0.06f }, { ID::drive, 0.35f }, { ID::wobble, 0.2f }, { ID::lofi, 0.35f }, { ID::haze, 0.1f }, { ID::room, 0 } }) },
        { "Guitar-Synth Lead", base ({ { ID::osc1Wave, 2 }, { ID::osc2Wave, 3 }, { ID::osc3Wave, 0 }, { ID::osc2Oct, 3 }, { ID::osc3Oct, 2 }, { ID::osc2Lvl, 0.5f }, { ID::osc3Lvl, 0.25f }, { ID::fat, 0.2f }, { ID::cutoff, 2600 }, { ID::reso, 0.2f }, { ID::envAmt, 0.5f }, { ID::fD, 0.6f }, { ID::fS, 0.3f }, { ID::aA, 0.02f }, { ID::aR, 0.5f }, { ID::mono, 1 }, { ID::glide, 0.09f }, { ID::tune, -5 }, { ID::wobble, 0.5f }, { ID::lofi, 0.4f }, { ID::haze, 0.5f }, { ID::room, 0.55f }, { ID::delayMix, 0.3f }, { ID::delayTime, 0.3f } }) },
        { "Dusty Stack Keys",  base ({ { ID::osc1Wave, 1 }, { ID::osc2Wave, 1 }, { ID::osc3Wave, 2 }, { ID::pw, 0.28f }, { ID::osc2Det, 6 }, { ID::fat, 0.25f }, { ID::cutoff, 2000 }, { ID::reso, 0.25f }, { ID::envAmt, 0.6f }, { ID::fD, 0.5f }, { ID::fS, 0.2f }, { ID::aD, 0.8f }, { ID::aS, 0.5f }, { ID::aR, 0.35f }, { ID::lfoRate, 5.2f }, { ID::lfoPitch, 0.05f }, { ID::wobble, 0.4f }, { ID::lofi, 0.5f }, { ID::haze, 0.45f }, { ID::room, 0.65f } }) },
        { "Night Drive Strings", base ({ { ID::osc1Wave, 0 }, { ID::osc2Wave, 0 }, { ID::osc3Wave, 0 }, { ID::osc2Det, 12 }, { ID::osc3Det, -10 }, { ID::osc3Oct, 2 }, { ID::fat, 1.0f }, { ID::cutoff, 2400 }, { ID::reso, 0 }, { ID::envAmt, 0.15f }, { ID::fA, 0.7f }, { ID::aA, 0.5f }, { ID::aR, 1.8f }, { ID::fR, 1.8f }, { ID::wobble, 0.3f }, { ID::lofi, 0.3f }, { ID::haze, 0.85f }, { ID::room, 0.3f }, { ID::delayMix, 0.15f }, { ID::delayTime, 0.5f } }) },
        { "Clean Fat Saw",     base ({ { ID::wobble, 0 }, { ID::lofi, 0 }, { ID::haze, 0 }, { ID::room, 0 } }) },
        { "Pluck Arp",         base ({ { ID::osc2Wave, 1 }, { ID::osc2Oct, 3 }, { ID::fat, 0.15f }, { ID::cutoff, 700 }, { ID::reso, 0.35f }, { ID::envAmt, 0.9f }, { ID::fD, 0.22f }, { ID::fS, 0 }, { ID::aD, 0.4f }, { ID::aS, 0 }, { ID::aR, 0.3f }, { ID::wobble, 0.25f }, { ID::lofi, 0.4f }, { ID::haze, 0.2f }, { ID::room, 0.3f }, { ID::delayMix, 0.4f }, { ID::delayFb, 0.45f } }) },
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
    p.osc2Detune = g (ID::osc2Det); p.osc3Detune = g (ID::osc3Det); p.pw = g (ID::pw); p.fat = g (ID::fat); p.fm = g (ID::fm); p.fmRatio = g (ID::fmRatio);
    p.cutoff = g (ID::cutoff); p.reso = g (ID::reso); p.envAmt = g (ID::envAmt); p.keyTrack = g (ID::keyTrack); p.filterDrive = g (ID::fDrive);
    p.fA = g (ID::fA); p.fD = g (ID::fD); p.fS = g (ID::fS); p.fR = g (ID::fR); p.aA = g (ID::aA); p.aD = g (ID::aD); p.aS = g (ID::aS); p.aR = g (ID::aR);
    p.lfoRate = g (ID::lfoRate); p.lfoToPitch = g (ID::lfoPitch); p.lfoToCutoff = g (ID::lfoCutoff); p.glide = g (ID::glide); p.mono = g (ID::mono) > 0.5f; p.tune = g (ID::tune);
    p.wobble = g (ID::wobble); p.lofi = g (ID::lofi); p.haze = g (ID::haze); p.room = g (ID::room); p.drive = g (ID::drive);
    p.delayTime = g (ID::delayTime); p.delayFb = g (ID::delayFb); p.delayMix = g (ID::delayMix);
    engine.setParams (p);

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
    polar::dsp::trackPeak (meterL, L, n); polar::dsp::trackPeak (meterR, R, n);
    energy.store (jlimit (0.0f, 1.0f, meterL.load() * 1.5f));
}

void McGeeProcessor::getStateInformation (MemoryBlock& dest) { if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest); }
void McGeeProcessor::setStateInformation (const void* data, int size) { if (auto xml = getXmlFromBinary (data, size)) apvts.replaceState (ValueTree::fromXml (*xml)); }
AudioProcessorEditor* McGeeProcessor::createEditor() { return new McGeeEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new McGeeProcessor(); }
