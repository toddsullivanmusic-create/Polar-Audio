#include "PluginProcessor.h"
#include "PluginEditor.h"

using P = juce::AudioParameterFloat;
using namespace juce;

AudioProcessorValueTreeState::ParameterLayout AuraProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (const String& id, const String& name, float def) {
        return std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 0.1f), def,
                                    AudioParameterFloatAttributes().withLabel ("%")); };
    l.add (pct (ID::mix, "Mix", 30.0f));
    l.add (pct (ID::size, "Size", 60.0f));
    l.add (std::make_unique<P> (ParameterID { ID::decay, 1 }, "Decay", NormalisableRange<float> (0.1f, 30.0f, 0.01f, 0.4f), 2.5f,
                                AudioParameterFloatAttributes().withLabel ("s")));
    l.add (std::make_unique<P> (ParameterID { ID::predelay, 1 }, "Pre-delay", NormalisableRange<float> (0.0f, 250.0f, 0.1f, 0.5f), 20.0f,
                                AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (pct (ID::damp, "Damping", 45.0f));
    l.add (std::make_unique<P> (ParameterID { ID::lowcut, 1 }, "Low Cut", NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.4f), 90.0f,
                                AudioParameterFloatAttributes().withLabel ("Hz")));
    l.add (pct (ID::diffusion, "Diffusion", 70.0f));
    l.add (pct (ID::modRate, "Mod Rate", 25.0f));
    l.add (pct (ID::modDepth, "Mod Depth", 35.0f));
    l.add (pct (ID::shimmer, "Shimmer", 0.0f));
    l.add (pct (ID::width, "Width", 100.0f));
    l.add (pct (ID::duck, "Duck", 0.0f));
    l.add (std::make_unique<P> (ParameterID { ID::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::colour, 1 }, "Colour",
                                                   StringArray { "Modern", "Vintage", "Dark", "Plate" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::freeze, 1 }, "Freeze", false));
    return l;
}

std::vector<polar::Preset> AuraProcessor::factoryPresets()
{
    using M = std::map<String, float>;
    return {
        { "Vocal Hall",        M { { ID::mix, 22 }, { ID::size, 62 }, { ID::decay, 2.4f }, { ID::predelay, 32 }, { ID::damp, 50 }, { ID::lowcut, 140 }, { ID::diffusion, 75 }, { ID::modRate, 20 }, { ID::modDepth, 30 }, { ID::shimmer, 0 }, { ID::width, 100 }, { ID::colour, 0 }, { ID::duck, 35 } } },
        { "Studio Plate",      M { { ID::mix, 25 }, { ID::size, 40 }, { ID::decay, 1.8f }, { ID::predelay, 12 }, { ID::damp, 25 }, { ID::lowcut, 180 }, { ID::diffusion, 95 }, { ID::modRate, 35 }, { ID::modDepth, 20 }, { ID::shimmer, 0 }, { ID::width, 100 }, { ID::colour, 3 }, { ID::duck, 0 } } },
        { "Cathedral",         M { { ID::mix, 35 }, { ID::size, 95 }, { ID::decay, 7.5f }, { ID::predelay, 60 }, { ID::damp, 55 }, { ID::lowcut, 100 }, { ID::diffusion, 80 }, { ID::modRate, 15 }, { ID::modDepth, 25 }, { ID::shimmer, 0 }, { ID::width, 100 }, { ID::colour, 0 }, { ID::duck, 0 } } },
        { "1982 Vintage Room", M { { ID::mix, 28 }, { ID::size, 50 }, { ID::decay, 2.0f }, { ID::predelay, 18 }, { ID::damp, 60 }, { ID::lowcut, 120 }, { ID::diffusion, 65 }, { ID::modRate, 40 }, { ID::modDepth, 55 }, { ID::shimmer, 0 }, { ID::width, 90 }, { ID::colour, 1 }, { ID::duck, 0 } } },
        { "Shimmer Heaven",    M { { ID::mix, 35 }, { ID::size, 90 }, { ID::decay, 9.0f }, { ID::predelay, 40 }, { ID::damp, 35 }, { ID::lowcut, 200 }, { ID::diffusion, 85 }, { ID::modRate, 20 }, { ID::modDepth, 40 }, { ID::shimmer, 65 }, { ID::width, 100 }, { ID::colour, 0 }, { ID::duck, 0 } } },
        { "Dark Ambient Wash", M { { ID::mix, 55 }, { ID::size, 100 }, { ID::decay, 18.0f }, { ID::predelay, 0 }, { ID::damp, 80 }, { ID::lowcut, 60 }, { ID::diffusion, 90 }, { ID::modRate, 10 }, { ID::modDepth, 60 }, { ID::shimmer, 20 }, { ID::width, 100 }, { ID::colour, 2 }, { ID::duck, 0 } } },
        { "Tight Drum Room",   M { { ID::mix, 18 }, { ID::size, 22 }, { ID::decay, 0.6f }, { ID::predelay, 5 }, { ID::damp, 40 }, { ID::lowcut, 150 }, { ID::diffusion, 85 }, { ID::modRate, 30 }, { ID::modDepth, 15 }, { ID::shimmer, 0 }, { ID::width, 80 }, { ID::colour, 0 }, { ID::duck, 0 } } },
        { "Worship Pad Bloom", M { { ID::mix, 50 }, { ID::size, 85 }, { ID::decay, 9.0f }, { ID::predelay, 30 }, { ID::damp, 45 }, { ID::lowcut, 110 }, { ID::diffusion, 85 }, { ID::modRate, 18 }, { ID::modDepth, 45 }, { ID::shimmer, 40 }, { ID::width, 100 }, { ID::colour, 1 }, { ID::duck, 20 } } },
    };
}

AuraProcessor::AuraProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AURA", createLayout()),
      presets (apvts, "AURA", factoryPresets())
{
}

bool AuraProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
    return layouts.getMainInputChannelSet() == out || layouts.getMainInputChannelSet() == AudioChannelSet::mono();
}

void AuraProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    wet.setSize (2, samplesPerBlock);
    outGain.prepare ({ sampleRate, (uint32) samplesPerBlock, 2 });
    outGain.setRampDurationSeconds (0.02);
    mixSm.reset (sampleRate, 0.03); widthSm.reset (sampleRate, 0.03);
    duckEnv.setLowpass (8.0f, sampleRate);
}

void AuraProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    auto get = [this] (const String& id) { return apvts.getRawParameterValue (id)->load(); };

    aura::ReverbEngine::Params p;
    p.predelayMs = get (ID::predelay); p.size = get (ID::size) * 0.01f; p.decaySec = get (ID::decay);
    p.damp = get (ID::damp) * 0.01f; p.lowcutHz = get (ID::lowcut); p.diffusion = get (ID::diffusion) * 0.01f;
    p.modRate = get (ID::modRate) * 0.01f; p.modDepth = get (ID::modDepth) * 0.01f; p.shimmer = get (ID::shimmer) * 0.01f;
    p.width = get (ID::width) * 0.01f; p.colour = (int) get (ID::colour); p.freeze = get (ID::freeze) > 0.5f;
    engine.setParams (p);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numCh > 1 ? buffer.getReadPointer (1) : inL;
    wet.setSize (2, n, false, false, true);
    engine.process (inL, inR, wet.getWritePointer (0), wet.getWritePointer (1), n);
    tailEnergy.store (engine.tailEnergy);

    mixSm.setTargetValue (get (ID::mix) * 0.01f);
    const float duckAmt = get (ID::duck) * 0.01f;
    float* wl = wet.getWritePointer (0); float* wr = wet.getWritePointer (1);
    for (int i = 0; i < n; ++i)
    {
        float m = mixSm.getNextValue();
        float dryLvl = std::abs (inL[i]) + std::abs (inR[i]);
        float env = duckEnv.lp (dryLvl);
        float duck = 1.0f - duckAmt * jlimit (0.0f, 1.0f, env * 3.0f);
        wl[i] *= m * duck; wr[i] *= m * duck;
    }
    float dry = 1.0f; // equal-power-ish dry retention
    for (int ch = 0; ch < numCh; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        const float* w = wet.getReadPointer (jmin (ch, 1));
        float m = mixSm.getCurrentValue();
        float dryGain = std::cos (m * 1.5707f) * dry + (1.0f - m) * 0.0f; dryGain = jmax (dryGain, 1.0f - m);
        for (int i = 0; i < n; ++i) d[i] = d[i] * dryGain + w[i];
    }
    outGain.setGainDecibels (get (ID::output));
    juce::dsp::AudioBlock<float> blk (buffer);
    outGain.process (juce::dsp::ProcessContextReplacing<float> (blk));
    polar::dsp::trackPeak (meterL, buffer.getReadPointer (0), n);
    polar::dsp::trackPeak (meterR, buffer.getReadPointer (numCh > 1 ? 1 : 0), n);
}

void AuraProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}
void AuraProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size)) apvts.replaceState (ValueTree::fromXml (*xml));
}

AudioProcessorEditor* AuraProcessor::createEditor() { return new AuraEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AuraProcessor(); }
