#pragma once
#include "DSP/Utils.h"
#include "ReverbEngine.h"   // AURA's reverb core powers the built-in space
#include <juce_dsp/juce_dsp.h>

namespace sanctuary
{
using namespace lumen::dsp;

enum Style { Warm = 0, Glass, Cathedral, Analog, Strings, kNumStyles };

struct PadParams
{
    int key = 0; bool minor = false; int style = Warm;
    float swell = 0.8f, air = 0.4f, depth = 0.5f, colour = 0.3f, motion = 0.4f, motionBars = 4;
    float transitionSec = 4.0f, fadeSec = 6.0f, reverb = 0.5f, width = 1.0f;
    bool play = true;
};

// One generated pad for a given key. ~26 oscillators in layered octaves, each with its own
// slow drift so nothing ever sits still. All synthesised: no samples, infinite sustain, any key.
class PadVoice
{
public:
    void prepare (double rate)
    {
        sr = rate;
        juce::dsp::ProcessSpec spec { rate, 512, 1 };
        filter.prepare (spec); filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass); filter.setResonance (0.4f);
        noiseLP.setLowpass (1200.0f, rate);
        for (int i = 0; i < kOsc; ++i) { drift[i].setRate (0.03f + 0.021f * (float) i, rate); drift[i].phase = (float) i / (float) kOsc; amp[i].setRate (0.05f + 0.013f * (float) i, rate); amp[i].phase = (float) (i * 7 % kOsc) / (float) kOsc; }
        gainSm.setTime (0.5f, rate);
    }

    void setKey (int key, bool minor, int style)
    {
        rootMidi = 36 + key; isMinor = minor; st = style;
        for (int i = 0; i < kOsc; ++i) { phase[i] = (float) (i * 37 % 100) / 100.0f; tri[i] = 0; }
    }

    void setTargetGain (float g, float seconds) { target = g; gainSm.setTime (juce::jmax (0.05f, seconds) * 0.35f, sr); }
    float currentGain() const { return gainSm.z; }
    bool  silent() const { return target <= 0.0001f && gainSm.z < 0.0005f; }

    inline float render (const PadParams& p, float cutoffHz)
    {
        float g = gainSm.process (target);
        if (g < 0.00005f) return 0.0f;

        const float root = (float) juce::MidiMessage::getMidiNoteInHertz (rootMidi);
        const float third = isMinor ? 1.189207f : 1.259921f;   // minor / major 3rd ratios
        const float fifth = 1.498307f;
        const float det = (st == Analog ? 0.006f : st == Strings ? 0.009f : st == Glass ? 0.0015f : 0.004f);

        float out = 0, hi = 0;
        int i = 0;
        auto osc = [&] (float ratio, float lvl, int wave, float oct)
        {
            float d = 1.0f + det * drift[i].sine();
            float f = root * ratio * oct * d;
            float inc = f / (float) sr;
            phase[i] += inc; if (phase[i] >= 1.0f) phase[i] -= 1.0f;
            float t = phase[i], v;
            if (wave == 0) { v = 2.0f * t - 1.0f; float dt = inc; if (t < dt) { float q = t / dt; v -= q + q - q * q - 1.0f; } else if (t > 1.0f - dt) { float q = (t - 1.0f) / dt; v -= q * q + q + q + 1.0f; } }
            else if (wave == 1) v = 1.0f - 4.0f * std::abs (t - 0.5f);
            else v = std::sin (t * 2.0f * kPi);
            float a = 0.75f + 0.25f * amp[i].sine();
            ++i;
            return v * lvl * a;
        };

        // --- foundation: sub sine + root octaves
        out += osc (1.0f, 0.9f * (0.4f + 0.6f * p.depth), 2, 0.5f);            // sub
        out += osc (1.0f, 0.5f, 2, 1.0f);
        int bodyWave = (st == Glass) ? 1 : (st == Cathedral ? 2 : 0);
        for (int k = 0; k < 4; ++k) out += osc (1.0f, 0.22f, bodyWave, 2.0f);  // detuned root cluster
        for (int k = 0; k < 3; ++k) out += osc (fifth, 0.17f, bodyWave, 2.0f);
        for (int k = 0; k < 2; ++k) out += osc (1.0f, 0.12f, bodyWave, 4.0f);
        for (int k = 0; k < 2; ++k) out += osc (fifth, 0.10f, bodyWave, 1.0f);
        // --- colour: the 3rd, kept gentle and an octave up so it never muddies the band
        for (int k = 0; k < 2; ++k) out += osc (third, 0.16f * p.colour, bodyWave == 0 ? 1 : bodyWave, 4.0f);
        // --- air: high sines/triangles with slow blooming amplitude (shimmer-like)
        for (int k = 0; k < 4; ++k) hi += osc (k < 2 ? 1.0f : fifth, 0.09f, k & 1 ? 1 : 2, k < 2 ? 8.0f : 8.0f);
        hi += osc (1.0f, 0.05f, 2, 16.0f);
        for (; i < kOsc; ++i) {}

        // --- breath: filtered noise (Strings / Cathedral get a touch more)
        float n = (rng.nextFloat() * 2.0f - 1.0f);
        float breath = noiseLP.lp (n) * (st == Strings ? 0.05f : st == Cathedral ? 0.04f : 0.02f) * p.air;

        float body = filter.processSample (0, out * 0.5f) + breath;
        float mix = body + hi * p.air * 1.4f;
        return mix * g;
    }

    void setCutoff (float hz) { filter.setCutoffFrequency (juce::jlimit (80.0f, 16000.0f, hz)); }

private:
    static constexpr int kOsc = 32;
    double sr = 44100; int rootMidi = 36; bool isMinor = false; int st = Warm;
    float phase[kOsc] {}, tri[kOsc] {};
    LFO drift[kOsc], amp[kOsc];
    juce::dsp::StateVariableTPTFilter<float> filter;
    OnePole noiseLP; juce::Random rng;
    Smooth gainSm; float target = 0;
};

class PadEngine
{
public:
    void prepare (double rate, int maxBlock)
    {
        sr = rate;
        for (auto& v : voices) v.prepare (rate);
        juce::dsp::ProcessSpec spec { rate, (juce::uint32) maxBlock, 2 };
        chorus.prepare (spec); chorus.setRate (0.22f); chorus.setDepth (0.35f); chorus.setCentreDelay (14.0f); chorus.setMix (0.5f);
        reverb.prepare (rate, maxBlock);
        motionLFO.setRate (0.05f, rate);
        swellSm.setTime (0.8f, rate); swellSm.reset (0);
        hp[0].setLowpass (30.0f, rate); hp[1].setLowpass (30.0f, rate);
        wetBuf.setSize (2, maxBlock);
        currentKey = -1;
    }

    void setParams (const PadParams& p, double bpm)
    {
        params = p;
        // key / mode / style change -> crossfade to the other voice
        int sig = p.key + (p.minor ? 12 : 0) + p.style * 24;
        if (sig != currentKey)
        {
            currentKey = sig;
            int next = 1 - active;
            voices[(size_t) next].setKey (p.key, p.minor, p.style);
            voices[(size_t) next].setTargetGain (1.0f, p.transitionSec);
            voices[(size_t) active].setTargetGain (0.0f, p.transitionSec);
            active = next;
            transitionProgress = 0;
        }
        float barsPerCycle = juce::jmax (1.0f, p.motionBars);
        motionLFO.setRate ((float) (bpm / 60.0) / (4.0f * barsPerCycle), sr);
        aura::ReverbEngine::Params rp;
        rp.size = p.style == Cathedral ? 0.95f : 0.8f; rp.decaySec = 4.0f + 8.0f * p.reverb; rp.damp = 0.5f; rp.lowcutHz = 150; rp.diffusion = 0.85f;
        rp.modRate = 0.2f; rp.modDepth = 0.4f; rp.shimmer = p.style == Glass ? 0.35f * p.air : 0.15f * p.air; rp.width = p.width; rp.colour = p.style == Analog ? 1 : 0;
        reverb.setParams (rp);
    }

    void render (float* L, float* R, int n)
    {
        const float styleCut = params.style == Warm ? 900.0f : params.style == Glass ? 4000.0f : params.style == Cathedral ? 1400.0f : params.style == Analog ? 1200.0f : 2200.0f;
        for (int i = 0; i < n; ++i)
        {
            float m = motionLFO.sine();
            float cut = styleCut * std::pow (2.0f, m * params.motion * 1.2f);
            for (auto& v : voices) v.setCutoff (cut);
            float x = 0;
            for (auto& v : voices) x += v.render (params, cut);
            float sw = swellSm.process (params.play ? params.swell : 0.0f);
            x = fastTanh (x * 0.9f) * sw;
            L[i] = x; R[i] = x;
        }
        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (blk));
        wetBuf.setSize (2, n, false, false, true);
        reverb.process (L, R, wetBuf.getWritePointer (0), wetBuf.getWritePointer (1), n);
        const float wet = 0.25f + 0.6f * params.reverb;
        const float* wl = wetBuf.getReadPointer (0); const float* wr = wetBuf.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            float l = hp[0].hp (L[i] * (1.0f - wet * 0.5f) + wl[i] * wet);
            float r = hp[1].hp (R[i] * (1.0f - wet * 0.5f) + wr[i] * wet);
            float mid = (l + r) * 0.5f, side = (l - r) * 0.5f * params.width;
            L[i] = mid + side; R[i] = mid - side;
        }
        transitionProgress = voices[(size_t) active].currentGain();
        level = reverb.tailEnergy;
    }

    void setFadeSeconds (float s) { swellSm.setTime (juce::jmax (0.05f, s) * 0.3f, sr); }
    float getTransitionProgress() const { return transitionProgress; }
    float getLevel() const { return level; }
    float getSwell() const { return swellSm.z; }

private:
    double sr = 44100; PadParams params; int currentKey = -1; int active = 0;
    std::array<PadVoice, 2> voices;
    juce::dsp::Chorus<float> chorus; aura::ReverbEngine reverb; juce::AudioBuffer<float> wetBuf;
    LFO motionLFO; Smooth swellSm; OnePole hp[2];
    float transitionProgress = 1, level = 0;
};
} // namespace sanctuary
