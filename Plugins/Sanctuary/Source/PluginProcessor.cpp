#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using P = AudioParameterFloat;
static const StringArray kKeys { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

AudioProcessorValueTreeState::ParameterLayout SanctuaryProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto f = [&] (const String& id, const String& name, float lo, float hi, float def, float skew = 1.0f, const String& unit = {})
    { l.add (std::make_unique<P> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.01f, skew), def, AudioParameterFloatAttributes().withLabel (unit))); };
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::key, 1 }, "Key", kKeys, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::mode, 1 }, "Mode", StringArray { "Major", "Minor" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::style, 1 }, "Style", StringArray { "Warm", "Glass", "Cathedral", "Analog", "Strings" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::play, 1 }, "Play", false));
    f (ID::swell, "Swell", 0, 100, 80, 1, "%"); f (ID::air, "Air", 0, 100, 40, 1, "%"); f (ID::depth, "Depth", 0, 100, 50, 1, "%");
    f (ID::colour, "Colour", 0, 100, 30, 1, "%"); f (ID::motion, "Motion", 0, 100, 40, 1, "%");
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ID::motionBars, 1 }, "Motion Cycle", StringArray { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 2));
    f (ID::transition, "Transition", 0.25f, 16.0f, 4.0f, 0.5f, "s"); f (ID::fade, "Fade", 0.25f, 30.0f, 6.0f, 0.5f, "s");
    f (ID::reverb, "Space", 0, 100, 50, 1, "%"); f (ID::width, "Width", 0, 100, 100, 1, "%"); f (ID::output, "Output", -24, 12, -3, 1, "dB");
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::midiKeys, 1 }, "MIDI Key Switch", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ID::modWheel, 1 }, "Mod Wheel > Swell", true));
    return l;
}

std::vector<lumen::Preset> SanctuaryProcessor::factoryPresets()
{
    using M = std::map<String, float>;
    return {
        { "Sunday Warm",        M { { ID::style, 0 }, { ID::air, 35 }, { ID::depth, 55 }, { ID::colour, 25 }, { ID::motion, 35 }, { ID::reverb, 50 }, { ID::transition, 4 } } },
        { "Glass Cathedral",    M { { ID::style, 1 }, { ID::air, 70 }, { ID::depth, 40 }, { ID::colour, 20 }, { ID::motion, 50 }, { ID::reverb, 80 }, { ID::transition, 6 } } },
        { "Deep Altar Call",    M { { ID::style, 2 }, { ID::air, 30 }, { ID::depth, 85 }, { ID::colour, 10 }, { ID::motion, 25 }, { ID::reverb, 90 }, { ID::transition, 8 } } },
        { "Analog Juno Bed",    M { { ID::style, 3 }, { ID::air, 25 }, { ID::depth, 50 }, { ID::colour, 45 }, { ID::motion, 45 }, { ID::reverb, 40 }, { ID::transition, 3 } } },
        { "String Swell",       M { { ID::style, 4 }, { ID::air, 50 }, { ID::depth, 45 }, { ID::colour, 55 }, { ID::motion, 60 }, { ID::reverb, 60 }, { ID::transition, 5 } } },
        { "Spontaneous Flow",   M { { ID::style, 0 }, { ID::air, 45 }, { ID::depth, 60 }, { ID::colour, 0 },  { ID::motion, 20 }, { ID::reverb, 70 }, { ID::transition, 10 } } },
        { "Prayer Room Drone",  M { { ID::style, 2 }, { ID::air, 15 }, { ID::depth, 100 }, { ID::colour, 0 }, { ID::motion, 10 }, { ID::reverb, 85 }, { ID::transition, 12 } } },
        { "Youth Night Bright", M { { ID::style, 1 }, { ID::air, 85 }, { ID::depth, 35 }, { ID::colour, 40 }, { ID::motion, 70 }, { ID::reverb, 55 }, { ID::transition, 2 } } },
    };
}

SanctuaryProcessor::SanctuaryProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SANCTUARY", createLayout()), presets (apvts, "SANCTUARY", factoryPresets()) {}

void SanctuaryProcessor::prepareToPlay (double sr, int block) { engine.prepare (sr, block); out.reset (sr, 0.02); }

void SanctuaryProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    auto g = [this] (const String& id) { return apvts.getRawParameterValue (id)->load(); };
    auto setParam = [this] (const String& id, float v) { if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };

    // --- Live control surface over MIDI (works from any keyboard or Logic's Musical Typing):
    //   notes C1..B1 (36-47) -> major key, C2..B2 (48-59) -> minor key, both start Play
    //   C3 (60) = Play/Stop toggle, D3 (62) = Fade out    |   mod wheel -> Swell   |   program change 0-23 -> key/mode
    if (g (ID::midiKeys) > 0.5f)
    {
        for (const auto meta : midi)
        {
            auto m = meta.getMessage();
            if (m.isNoteOn())
            {
                int note = m.getNoteNumber();
                if (note >= 36 && note <= 59) { setParam (ID::key, (float) (note % 12)); setParam (ID::mode, note >= 48 ? 1.0f : 0.0f); setParam (ID::play, 1.0f); }
                else if (note == 60) setParam (ID::play, g (ID::play) > 0.5f ? 0.0f : 1.0f);
                else if (note == 62) setParam (ID::play, 0.0f);
            }
            else if (m.isProgramChange() && m.getProgramChangeNumber() < 24)
            { int pc = m.getProgramChangeNumber(); setParam (ID::key, (float) (pc % 12)); setParam (ID::mode, pc >= 12 ? 1.0f : 0.0f); setParam (ID::play, 1.0f); }
            else if (m.isController() && m.getControllerNumber() == 1 && g (ID::modWheel) > 0.5f)
                setParam (ID::swell, (float) m.getControllerValue() / 127.0f * 100.0f);
        }
    }

    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) hostBpm.store (*bpm);

    sanctuary::PadParams p;
    p.key = (int) g (ID::key); p.minor = g (ID::mode) > 0.5f; p.style = (int) g (ID::style); p.play = g (ID::play) > 0.5f;
    p.swell = g (ID::swell) * 0.01f; p.air = g (ID::air) * 0.01f; p.depth = g (ID::depth) * 0.01f; p.colour = g (ID::colour) * 0.01f;
    p.motion = g (ID::motion) * 0.01f; p.motionBars = std::pow (2.0f, g (ID::motionBars)); p.transitionSec = g (ID::transition);
    p.fadeSec = g (ID::fade); p.reverb = g (ID::reverb) * 0.01f; p.width = g (ID::width) * 0.01f;
    engine.setFadeSeconds (p.fadeSec);
    engine.setParams (p, hostBpm.load());

    buffer.clear();
    float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);
    engine.render (L, R, n);
    out.setTargetValue (Decibels::decibelsToGain (g (ID::output)));
    for (int i = 0; i < n; ++i) { float v = out.getNextValue(); L[i] *= v; R[i] *= v; }
    lumen::dsp::trackPeak (meterL, L, n); lumen::dsp::trackPeak (meterR, R, n);
    energy.store (jlimit (0.0f, 1.0f, engine.getLevel() * 3.0f));
    transition.store (engine.getTransitionProgress());
    swellLevel.store (engine.getSwell());
}

void SanctuaryProcessor::getStateInformation (MemoryBlock& dest) { if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest); }
void SanctuaryProcessor::setStateInformation (const void* data, int size) { if (auto xml = getXmlFromBinary (data, size)) apvts.replaceState (ValueTree::fromXml (*xml)); }
AudioProcessorEditor* SanctuaryProcessor::createEditor() { return new SanctuaryEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SanctuaryProcessor(); }
