#pragma once
#include "DSP/Utils.h"
#include <array>

namespace aura
{
using namespace lumen::dsp;

// AURA core: pre-delay -> input filtering -> 4-stage modulated diffusion -> 8-line Householder FDN
// with per-line damping, slow chorused modulation, optional octave shimmer in the feedback path,
// and three "colour" circuits that change the character of the loop itself (not a post-EQ).
class ReverbEngine
{
public:
    enum Colour { Modern = 0, Vintage, Dark, Plate };

    struct Params
    {
        float predelayMs = 20, size = 0.6f, decaySec = 2.5f, damp = 0.5f, lowcutHz = 80,
              diffusion = 0.7f, modRate = 0.6f, modDepth = 0.4f, shimmer = 0.0f, width = 1.0f;
        int colour = Modern; bool freeze = false;
    };

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        juce::ignoreUnused (maxBlock);
        predelay[0].prepare ((int) (0.3 * sr)); predelay[1].prepare ((int) (0.3 * sr));
        for (auto& a : diffL) a.prepare ((int) (0.05 * sr));
        for (auto& a : diffR) a.prepare ((int) (0.05 * sr));
        for (auto& d : lines) d.prepare ((int) (0.25 * sr) + 64);
        shimL.prepare ((int) (0.12 * sr) + 8); shimR.prepare ((int) (0.12 * sr) + 8);
        for (int i = 0; i < 8; ++i)
        {
            lfo[i].setRate (0.1f, sr); lfo[i].phase = (float) i / 8.0f;
            dampLP[i].z = 0; lineZ[i] = 0;
        }
        for (int i = 0; i < 2; ++i) { inLowcut[i].setLowpass (80.0f, sr); inHighcut[i].setLowpass (18000.0f, sr); vintageLP[i].setLowpass (9000.0f, sr); }
        sizeSmooth.setTime (0.15f, sr); sizeSmooth.reset (0.6f);
        shimPhase = 0; reset();
    }

    void reset()
    {
        for (auto& d : lines) d.clear();
        for (auto& a : diffL) a.dl.clear();
        for (auto& a : diffR) a.dl.clear();
        predelay[0].clear(); predelay[1].clear(); shimL.clear(); shimR.clear();
        for (auto& z : lineZ) z = 0; for (auto& d : dampLP) d.z = 0;
    }

    void setParams (const Params& p)
    {
        params = p;
        for (int i = 0; i < 2; ++i) inLowcut[i].setLowpass (p.lowcutHz, sr);
        float dampHz = juce::jmap (1.0f - p.damp, 0.0f, 1.0f, 1200.0f, 16000.0f);
        if (p.colour == Dark) dampHz *= 0.45f;
        if (p.colour == Plate) dampHz = juce::jmax (dampHz, 9000.0f);
        for (auto& d : dampLP) d.setLowpass (dampHz, sr);
        float rate = juce::jmap (p.modRate, 0.05f, 3.0f);
        for (int i = 0; i < 8; ++i) lfo[i].setRate (rate * (0.83f + 0.05f * (float) i), sr);
        diffG = juce::jmap (p.diffusion, 0.0f, 0.78f);
        if (p.colour == Plate) diffG = juce::jmax (diffG, 0.7f);
    }

    // Returns reverb energy (for UI) via tailEnergy
    void process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        const float pdSamples = params.predelayMs * 0.001f * (float) sr;
        const float sizeTarget = juce::jmap (params.size, 0.0f, 1.0f, 0.18f, 1.0f);
        const float modDepthSamples = params.modDepth * 12.0f * (float) (sr / 44100.0);
        const bool vintage = params.colour == Vintage;
        float energy = 0;

        for (int s = 0; s < n; ++s)
        {
            // --- pre-delay + input filtering
            predelay[0].push (inL[s]); predelay[1].push (inR[s]);
            float l = predelay[0].read (pdSamples), r = predelay[1].read (pdSamples);
            l = inLowcut[0].hp (l); r = inLowcut[1].hp (r);
            if (params.freeze) { l = 0; r = 0; }

            // --- diffusion (modulated series allpasses, L/R get slightly different lengths)
            const float sz = sizeSmooth.process (sizeTarget);
            static constexpr float dlen[4] = { 142.0f, 107.0f, 379.0f, 277.0f };
            for (int i = 0; i < 4; ++i)
            {
                float m = lfo[i].sine() * modDepthSamples * 0.25f;
                diffL[i].g = diffG; diffR[i].g = diffG;
                float lenL = dlen[i] * sz * (float) (sr / 44100.0) + m;
                float lenR = dlen[i] * 1.07f * sz * (float) (sr / 44100.0) - m;
                l = diffL[i].process (l, juce::jmax (2.0f, lenL));
                r = diffR[i].process (r, juce::jmax (2.0f, lenR));
            }

            // --- FDN: read, damp, mix (Householder), feed back
            static constexpr float base[8] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 }; // mutually prime-ish @44.1k
            float v[8];
            float decayGain[8];
            for (int i = 0; i < 8; ++i)
            {
                float len = base[i] * (0.45f + 0.95f * sz) * (float) (sr / 44100.0);
                float mod = lfo[4 + (i & 3)].sine() * modDepthSamples * ((i & 1) ? 1.0f : -0.8f);
                float x = lines[i].read (juce::jmax (4.0f, len + mod));
                x = dampLP[i].lp (x);
                if (vintage) x = fastTanh (x * 1.3f) * 0.77f;   // gentle loop saturation = "bloom"
                float rt = params.freeze ? 1.0f : std::pow (10.0f, -3.0f * (len / (float) sr) / juce::jmax (0.05f, params.decaySec));
                decayGain[i] = juce::jmin (rt, 0.9999f);
                v[i] = x * decayGain[i];
            }
            float sum = 0; for (float x : v) sum += x;
            const float hh = -2.0f / 8.0f;
            float fb[8]; for (int i = 0; i < 8; ++i) fb[i] = v[i] + hh * sum;

            // --- shimmer: octave-up granular pitch shift of the loop sum, re-injected
            float shimOutL = 0, shimOutR = 0;
            if (params.shimmer > 0.0005f)
            {
                shimL.push (v[0] + v[2] + v[4] + v[6]); shimR.push (v[1] + v[3] + v[5] + v[7]);
                const float win = 0.09f * (float) sr;
                shimPhase += 1.0f / win; if (shimPhase >= 1.0f) shimPhase -= 1.0f;
                float ph2 = shimPhase + 0.5f; if (ph2 >= 1.0f) ph2 -= 1.0f;
                auto tap = [&] (DelayLine& d, float ph) { float env = std::sin (ph * kPi); return d.read (1.0f + ph * win) * env; };
                shimOutL = (tap (shimL, shimPhase) + tap (shimL, ph2)) * 0.5f;
                shimOutR = (tap (shimR, shimPhase) + tap (shimR, ph2)) * 0.5f;
                // Re-injection is scaled by the loop's remaining headroom (1 - decay gain) so the loop can never exceed unity
                // on long decays, and the loop is soft-limited so a hot shimmer blooms instead of running away.
                float amt = params.shimmer * (params.freeze ? 0.15f : 1.0f);
                for (int i = 0; i < 8; ++i)
                {
                    float head = juce::jlimit (0.02f, 0.35f, (1.0f - decayGain[i]) * 6.0f + 0.02f);
                    fb[i] = fastTanh (fb[i] + ((i & 1) ? shimOutR : shimOutL) * amt * head);
                }
            }

            float inj = 0.5f;
            for (int i = 0; i < 8; ++i)
            {
                float in = (i & 1) ? r : l;
                float val = fb[i] + in * inj;
                if (vintage) val = vintageLP[i & 1].lp (val);
                lines[i].push (val);
            }

            float wl = (v[0] + v[2] + v[4] + v[6]) * 0.45f + (v[1] - v[3]) * 0.1f;
            float wr = (v[1] + v[3] + v[5] + v[7]) * 0.45f + (v[0] - v[2]) * 0.1f;
            wl += shimOutL * params.shimmer * 0.35f; wr += shimOutR * params.shimmer * 0.35f;

            // width
            float mid = (wl + wr) * 0.5f, side = (wl - wr) * 0.5f * params.width;
            outL[s] = mid + side; outR[s] = mid - side;
            energy += wl * wl + wr * wr;
        }
        tailEnergy = std::sqrt (energy / (float) juce::jmax (1, n));
    }

    float tailEnergy = 0;

private:
    double sr = 44100.0;
    Params params;
    DelayLine predelay[2];
    Allpass diffL[4], diffR[4];
    float diffG = 0.6f;
    std::array<DelayLine, 8> lines;
    std::array<OnePole, 8> dampLP;
    std::array<float, 8> lineZ {};
    OnePole inLowcut[2], inHighcut[2], vintageLP[2];
    LFO lfo[8];
    Smooth sizeSmooth;
    DelayLine shimL, shimR; float shimPhase = 0;
};
} // namespace aura
