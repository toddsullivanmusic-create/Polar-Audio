#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using P = AudioParameterFloat;

AudioProcessorValueTreeState::ParameterLayout GlowProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<P> (ParameterID { ID::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 36.0f, 0.1f), 6.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<P> (ParameterID { ID::bias, 1 }, "Bias", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<P> (ParameterID { ID::focus, 1 }, "Focus", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<P> (ParameterID { ID::dynamics, 1 }, "Dynamics", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<P> (ParameterID { ID::tone, 1 }, "Tone", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<P> (ParameterID { ID::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<P> (ParameterID { ID::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::circuit, 1 }, "Circuit", StringArray { "Tube", "Tape", "Transformer", "Crunch" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::oversample, 1 }, "Oversampling", StringArray { "2x", "4x" }, 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::autogain, 1 }, "Auto Gain", true));
    return l;
}

std::vector<polar::Preset> GlowProcessor::factoryPresets()
{
    using M = std::map<String, float>;
    return {
        { "Vocal Warmth",     M { { ID::drive, 5 },  { ID::bias, 35 }, { ID::focus, 10 },  { ID::dynamics, 20 }, { ID::tone, 0.5f },  { ID::mix, 100 }, { ID::circuit, 0 } } },
        { "Tape Bus Glue",    M { { ID::drive, 4 },  { ID::bias, 0 },  { ID::focus, 0 },   { ID::dynamics, 35 }, { ID::tone, -0.5f }, { ID::mix, 100 }, { ID::circuit, 1 } } },
        { "Iron Low End",     M { { ID::drive, 10 }, { ID::bias, 0 },  { ID::focus, -70 }, { ID::dynamics, 0 },  { ID::tone, -1 },    { ID::mix, 100 }, { ID::circuit, 2 } } },
        { "Silk Top",         M { { ID::drive, 8 },  { ID::bias, 20 }, { ID::focus, 80 },  { ID::dynamics, 0 },  { ID::tone, 1.5f },  { ID::mix, 60 },  { ID::circuit, 0 } } },
        { "Drum Crunch",      M { { ID::drive, 18 }, { ID::bias, 0 },  { ID::focus, 20 },  { ID::dynamics, 50 }, { ID::tone, 1 },     { ID::mix, 45 },  { ID::circuit, 3 } } },
        { "Parallel Thick",   M { { ID::drive, 24 }, { ID::bias, 40 }, { ID::focus, -30 }, { ID::dynamics, 0 },  { ID::tone, -2 },    { ID::mix, 35 },  { ID::circuit, 0 } } },
        { "Mastering Touch",  M { { ID::drive, 2 },  { ID::bias, 10 }, { ID::focus, 0 },   { ID::dynamics, 10 }, { ID::tone, 0 },     { ID::mix, 100 }, { ID::circuit, 1 } } },
        { "Synth Destroyer",  M { { ID::drive, 34 }, { ID::bias, 60 }, { ID::focus, 0 },   { ID::dynamics, 0 },  { ID::tone, 2 },     { ID::mix, 100 }, { ID::circuit, 3 } } },
    };
}

GlowProcessor::GlowProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GLOW", createLayout()), presets (apvts, "GLOW", factoryPresets())
{
    os2 = std::make_unique<dsp::Oversampling<float>> (2, 1, dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    os4 = std::make_unique<dsp::Oversampling<float>> (2, 2, dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
}

bool GlowProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return (out == AudioChannelSet::stereo() || out == AudioChannelSet::mono()) && layouts.getMainInputChannelSet() == out;
}

void GlowProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    os2->initProcessing ((size_t) samplesPerBlock); os4->initProcessing ((size_t) samplesPerBlock);
    os2->reset(); os4->reset();
    currentOS = -1;
    dryBuf.setSize (2, samplesPerBlock);
    for (int ch = 0; ch < 2; ++ch) { tiltLow[ch].reset(); tiltHigh[ch].reset(); }
    mixSm.reset (sampleRate, 0.02); outSm.reset (sampleRate, 0.02); agSm.reset (sampleRate, 0.1);
    rmsIn = rmsOut = 0;
}

void GlowProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), numCh = jmin (2, buffer.getNumChannels());
    auto get = [this] (const String& id) { return apvts.getRawParameterValue (id)->load(); };

    int osSel = (int) get (ID::oversample);
    auto& os = osSel == 0 ? *os2 : *os4;
    if (osSel != currentOS)
    {
        currentOS = osSel;
        engine.prepare (sr * (osSel == 0 ? 2.0 : 4.0), numCh);
        setLatencySamples ((int) os.getLatencyInSamples());
    }

    glow::SaturationEngine::Params p;
    p.driveDb = get (ID::drive); p.bias = get (ID::bias) * 0.01f; p.focus = get (ID::focus) * 0.01f;
    p.dynamics = get (ID::dynamics) * 0.01f; p.circuit = (int) get (ID::circuit);
    engine.setParams (p);

    dryBuf.setSize (2, n, false, false, true);
    for (int ch = 0; ch < numCh; ++ch) dryBuf.copyFrom (ch, 0, buffer, ch, 0, n);
    polar::dsp::trackPeak (inputLevel, buffer.getReadPointer (0), n);

    float inSq = 0; for (int ch = 0; ch < numCh; ++ch) { auto* d = buffer.getReadPointer (ch); for (int i = 0; i < n; ++i) inSq += d[i] * d[i]; }

    dsp::AudioBlock<float> blk (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
    auto up = os.processSamplesUp (blk);
    for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
    {
        float* d = up.getChannelPointer (ch);
        for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = engine.processSample (d[i], (int) ch);
    }
    os.processSamplesDown (blk);

    // Tilt tone: low shelf and high shelf in opposite directions around 700 Hz
    float tilt = get (ID::tone);
    auto lowC  = dsp::IIR::Coefficients<float>::makeLowShelf (sr, 700.0, 0.6f, Decibels::decibelsToGain (-tilt));
    auto highC = dsp::IIR::Coefficients<float>::makeHighShelf (sr, 700.0, 0.6f, Decibels::decibelsToGain (tilt));
    for (int ch = 0; ch < numCh; ++ch)
    {
        tiltLow[ch].coefficients = lowC; tiltHigh[ch].coefficients = highC;
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i) d[i] = tiltHigh[ch].processSample (tiltLow[ch].processSample (d[i]));
    }

    // Auto gain: slow RMS match of wet vs dry
    float outSq = 0; for (int ch = 0; ch < numCh; ++ch) { auto* d = buffer.getReadPointer (ch); for (int i = 0; i < n; ++i) outSq += d[i] * d[i]; }
    const float a = 0.15f;
    rmsIn += a * (inSq - rmsIn); rmsOut += a * (outSq - rmsOut);
    float ag = 1.0f;
    if (get (ID::autogain) > 0.5f && rmsOut > 1e-7f && rmsIn > 1e-7f)
        ag = jlimit (0.25f, 4.0f, std::sqrt (rmsIn / rmsOut));
    agSm.setTargetValue (ag);
    mixSm.setTargetValue (get (ID::mix) * 0.01f);
    outSm.setTargetValue (Decibels::decibelsToGain (get (ID::output)));

    float e = 0;
    for (int i = 0; i < n; ++i)
    {
        float m = mixSm.getNextValue(), g = agSm.getNextValue() * outSm.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            float wet = d[i] * agSm.getCurrentValue();
            float dry = dryBuf.getSample (ch, i);
            d[i] = (dry + (wet - dry) * m) * (g / jmax (0.001f, agSm.getCurrentValue()));
            e += std::abs (wet - dry);
        }
    }
    energy.store (jlimit (0.0f, 1.0f, e / (float) (n * numCh) * 4.0f));
    polar::dsp::trackPeak (meterL, buffer.getReadPointer (0), n);
    polar::dsp::trackPeak (meterR, buffer.getReadPointer (numCh > 1 ? 1 : 0), n);
}

void GlowProcessor::getStateInformation (MemoryBlock& dest) { if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest); }
void GlowProcessor::setStateInformation (const void* data, int size) { if (auto xml = getXmlFromBinary (data, size)) apvts.replaceState (ValueTree::fromXml (*xml)); }
AudioProcessorEditor* GlowProcessor::createEditor() { return new GlowEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GlowProcessor(); }
