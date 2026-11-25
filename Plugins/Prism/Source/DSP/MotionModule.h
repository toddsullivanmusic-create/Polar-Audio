#pragma once
#include "Module.h"

namespace prism
{
// MOTION — modulation. All delay-based types use Hermite-interpolated reads, so even fast, deep
// sweeps stay free of zipper noise and HF loss.
class MotionModule : public Module
{
public:
    enum Type { Chorus = 0, Vibrato, Phaser, Tremolo, Flanger, Detune, kNumTypes };

    void prepare (double sampleRate, int maxBlock) override
    {
        prepareBase (sampleRate, maxBlock);
        for (int c = 0; c < 2; ++c)
        {
            chorusD[c].prepare ((int) (sampleRate * 0.05)); vibD[c].prepare ((int) (sampleRate * 0.03));
            flangeD[c].prepare ((int) (sampleRate * 0.02)); shift[c].prepare (sampleRate, 38.0f);
            toneLP[c].setLowpass (12000.0f, sampleRate);
        }
        for (int t = 0; t < kNumTypes; ++t) resetType (t);
    }

    static float rateHz (float tw) { return 0.05f * std::pow (2.0f, tw * 8.0f); }   // 0.05 .. 12.8 Hz

protected:
    void resetType (int t) override
    {
        for (int c = 0; c < 2; ++c)
        {
            if (t == Chorus) chorusD[c].clear();
            if (t == Vibrato) vibD[c].clear();
            if (t == Flanger) { flangeD[c].clear(); flangeFb[c] = 0; }
            if (t == Detune) shift[c].clear();
            if (t == Phaser) { for (auto& z : apZ[c]) z = 0; phFb[c] = 0; }
        }
    }

    inline float advance (float hz) { phase += hz / (float) sr; if (phase >= 1.0f) phase -= 1.0f; return phase; }

    void processType (int type, float* L, float* R, int n) override
    {
        const float hz = rateHz (tw);
        const float ms = (float) sr * 0.001f;
        switch (type)
        {
            case Chorus:
            {
                const float depth = amt * 7.0f * ms, base = 9.0f * ms;
                for (int c = 0; c < 2; ++c) toneLP[c].setLowpass (2000.0f * std::pow (2.0f, tn * 3.2f), sr);
                for (int i = 0; i < n; ++i)
                {
                    const float p = advance (hz * 0.25f + 0.02f);
                    float* ch[2] = { L, R };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float x = ch[c][i];
                        chorusD[c].push (x);
                        float w = 0.0f;
                        for (int v = 0; v < 3; ++v)
                        {
                            const float ph = p + (float) v / 3.0f + (c ? 0.25f : 0.0f);
                            w += chorusD[c].read (base + (float) v * 2.3f * ms + depth * (0.5f + 0.5f * std::sin (ph * 2.0f * kPi)));
                        }
                        ch[c][i] = x * 0.62f + toneLP[c].lp (w * (1.0f / 3.0f)) * 0.78f;
                    }
                }
                break;
            }
            case Vibrato:
            {
                const float depth = amt * 4.5f * ms, base = 1.0f * ms + depth;
                const float walkAmt = tn;
                for (int i = 0; i < n; ++i)
                {
                    const float p = advance (hz);
                    if (noise.uni() < hz * 4.0f / (float) sr) walkTarget = noise.next();
                    walk += 0.0008f * (walkTarget - walk);
                    const float mod = std::sin (p * 2.0f * kPi) * (1.0f - walkAmt) + walk * walkAmt * 1.6f;
                    const float d = base + depth * juce::jlimit (-1.0f, 1.0f, mod);
                    vibD[0].push (L[i]); vibD[1].push (R[i]);
                    L[i] = vibD[0].read (d); R[i] = vibD[1].read (d);
                }
                break;
            }
            case Phaser:
            {
                const float fb = tn * 0.86f;
                for (int i = 0; i < n; ++i)
                {
                    const float p = advance (hz * 0.5f);
                    float* ch[2] = { L, R };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float lfo = 0.5f + 0.5f * std::sin ((p + (c ? 0.18f : 0.0f)) * 2.0f * kPi);
                        const float f = 180.0f * std::pow (2.0f, lfo * (1.0f + amt * 4.5f));
                        const float t = std::tan (kPi * juce::jmin (f, (float) sr * 0.45f) / (float) sr);
                        const float a = (t - 1.0f) / (t + 1.0f);
                        const float x = ch[c][i];
                        float y = x + phFb[c] * fb;
                        for (int s = 0; s < 6; ++s)
                        {
                            const float o = a * y + apZ[c][s];
                            apZ[c][s] = y - a * o;
                            y = o;
                        }
                        phFb[c] = fastTanh (y);
                        ch[c][i] = (x + y) * 0.6f;
                    }
                }
                break;
            }
            case Tremolo:
            {
                const float k = 1.0f + tn * 12.0f, nk = 1.0f / fastTanh (k);
                for (int i = 0; i < n; ++i)
                {
                    const float p = advance (hz);
                    const float sl = fastTanh (k * std::sin (p * 2.0f * kPi)) * nk;
                    const float sr2 = fastTanh (k * std::sin ((p + 0.5f * tn) * 2.0f * kPi)) * nk;
                    L[i] *= 1.0f - amt * (0.5f - 0.5f * sl);
                    R[i] *= 1.0f - amt * (0.5f - 0.5f * sr2);
                }
                break;
            }
            case Flanger:
            {
                const float fb = (tn - 0.5f) * 1.9f, depth = amt * 6.0f * ms, base = 0.3f * ms;
                for (int i = 0; i < n; ++i)
                {
                    const float p = advance (hz * 0.3f);
                    float* ch[2] = { L, R };
                    for (int c = 0; c < 2; ++c)
                    {
                        const float lfo = 0.5f + 0.5f * std::sin ((p + (c ? 0.25f : 0.0f)) * 2.0f * kPi);
                        const float x = ch[c][i];
                        flangeD[c].push (x + flangeFb[c] * fb);
                        const float d = flangeD[c].read (base + depth * lfo + 3.0f);
                        flangeFb[c] = fastTanh (d);
                        ch[c][i] = (x + d) * 0.6f;
                    }
                }
                break;
            }
            case Detune:
            {
                for (int c = 0; c < 2; ++c) toneLP[c].setLowpass (2500.0f * std::pow (2.0f, tn * 3.0f), sr);
                for (int i = 0; i < n; ++i)
                {
                    const float p = advance (0.13f + tw * 0.6f);
                    const float cents = amt * 28.0f * (1.0f + 0.35f * tw * std::sin (p * 2.0f * kPi));
                    const float up = std::pow (2.0f, cents / 1200.0f);
                    const float sl = shift[0].process (L[i], up), sr2 = shift[1].process (R[i], 1.0f / up);
                    L[i] = L[i] * 0.72f + toneLP[0].lp (sl) * 0.72f;
                    R[i] = R[i] * 0.72f + toneLP[1].lp (sr2) * 0.72f;
                }
                break;
            }
            default: break;
        }
        uiA.store (phase);
    }

private:
    HDelay chorusD[2], vibD[2], flangeD[2];
    PitchShifter shift[2];
    OnePole toneLP[2];
    float apZ[2][6] {}, phFb[2] {}, flangeFb[2] {};
    float phase = 0, walk = 0, walkTarget = 0;
    Noise noise;
};
} // namespace prism
