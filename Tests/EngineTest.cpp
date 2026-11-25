#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000; const int block = 256;
    const int seconds = 6;
    juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory();
    std::unique_ptr<juce::AudioProcessor> proc (createPluginFilter());
    proc->setPlayConfigDetails (proc->acceptsMidi() ? 0 : 2, 2, sr, block);
    proc->prepareToPlay (sr, block);
    juce::String name = proc->getName();
    std::printf ("[%s] params: %d  editor: %s\n", name.toRawUTF8(), proc->getParameters().size(), proc->hasEditor() ? "yes" : "no");

    // Walk every factory preset: render, check health
    auto* programs = proc.get();
    int presetCount = 1;
   #if POLAR_TEST_Aura
    presetCount = (int) dynamic_cast<AuraProcessor*> (programs)->presets.factoryPresets.size();
   #elif POLAR_TEST_Glow
    presetCount = (int) dynamic_cast<GlowProcessor*> (programs)->presets.factoryPresets.size();
   #elif POLAR_TEST_McGee
    presetCount = (int) dynamic_cast<McGeeProcessor*> (programs)->presets.factoryPresets.size();
   #elif POLAR_TEST_Sanctuary
    presetCount = (int) dynamic_cast<SanctuaryProcessor*> (programs)->presets.factoryPresets.size();
   #elif POLAR_TEST_Prism
    // every factory preset, then every effect type of every module at a hot setting (6 extra passes)
    presetCount = (int) dynamic_cast<PrismProcessor*> (programs)->presets.factoryPresets.size() + 6;
   #endif

    bool ok = true;
    for (int preset = 0; preset < presetCount; ++preset)
    {
       #if POLAR_TEST_Aura
        dynamic_cast<AuraProcessor*> (programs)->presets.applyIndex (preset);
       #elif POLAR_TEST_Glow
        dynamic_cast<GlowProcessor*> (programs)->presets.applyIndex (preset);
       #elif POLAR_TEST_McGee
        dynamic_cast<McGeeProcessor*> (programs)->presets.applyIndex (preset);
       #elif POLAR_TEST_Prism
        {
            auto* pp = dynamic_cast<PrismProcessor*> (programs);
            const int nf = (int) pp->presets.factoryPresets.size();
            if (preset < nf) pp->presets.applyIndex (preset);
            else
                for (int m = 0; m < 4; ++m)
                {
                    auto set = [&] (const juce::String& suf, float v) { auto* prm = pp->apvts.getParameter (PID::m (m, suf)); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
                    set (PID::type, (float) (preset - nf)); set (PID::amount, 0.85f); set (PID::tweak, 0.6f); set (PID::tone, 0.7f);
                    set (PID::mix, 0.6f); set (PID::on, 1.0f);
                }
        }
       #elif POLAR_TEST_Sanctuary
        auto* sp = dynamic_cast<SanctuaryProcessor*> (programs);
        sp->presets.applyIndex (preset);
        if (auto* p = sp->apvts.getParameter ("play")) p->setValueNotifyingHost (1.0f);
        if (auto* p = sp->apvts.getParameter ("transition")) p->setValueNotifyingHost (p->convertTo0to1 (1.0f));
       #endif
        proc->reset(); proc->prepareToPlay (sr, block);
        const int total = (int) (sr * seconds);
        juce::AudioBuffer<float> render (2, total);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        double phase = 0;
        for (int pos = 0; pos < total; pos += block)
        {
            buf.clear(); midi.clear();
            if (! proc->acceptsMidi())
            {
                // test signal: 2 s of 110 Hz + 1 kHz tone at -12 dBFS, then a decaying burst, then silence (tail check)
                for (int i = 0; i < block; ++i)
                {
                    double t = (pos + i) / sr;
                    float v = t < 2.0 ? 0.25f * (float) (std::sin (phase) + 0.5 * std::sin (phase * 9.09)) : (t < 2.02 ? 0.8f : 0.0f);
                    phase += 2 * juce::MathConstants<double>::pi * 110.0 / sr;
                    buf.setSample (0, i, v); buf.setSample (1, i, v * 0.9f);
                }
            }
            else
            {
               #if POLAR_TEST_McGee
                if (pos == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.9f), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 55, 0.8f), 0); midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0); }
                if (pos == (int) (sr * 2.0) / block * block) { midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0); midi.addEvent (juce::MidiMessage::noteOff (1, 55), 0); midi.addEvent (juce::MidiMessage::noteOff (1, 64), 0); }
               #elif POLAR_TEST_Sanctuary
                if (pos == (int) (sr * 3.0) / block * block) midi.addEvent (juce::MidiMessage::noteOn (1, 43, 0.9f), 0); // key switch to G major mid-render
               #endif
            }
            proc->processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch) render.copyFrom (ch, pos, buf, ch, 0, juce::jmin (block, total - pos));
        }
        float peak = render.getMagnitude (0, total), rms = render.getRMSLevel (0, 0, total);
        bool nan = false; for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < total; ++i) if (! std::isfinite (render.getSample (ch, i))) nan = true;
        float tail = render.getRMSLevel (0, (int) (sr * 5.5), (int) (sr * 0.5));
        bool good = ! nan && peak > 0.001f && peak < 1.5f;
        ok &= good;
        std::printf ("  preset %d: peak %.3f  rms %.4f (%.1f dBFS)  tail@5.5s %.5f  %s\n", preset, peak, rms, juce::Decibels::gainToDecibels (rms), tail, nan ? "NaN!" : good ? "OK" : "SUSPECT");
        if (preset == 0) writeWav (outDir.getChildFile (name + "_preset0.wav"), render, sr);
    }
    proc->releaseResources();
    std::printf ("[%s] %s\n", name.toRawUTF8(), ok ? "ALL GOOD" : "PROBLEMS");
    return ok ? 0 : 1;
}
