#pragma once
#include "Module.h"

namespace prism
{
// COLOR — drive, dirt and dynamics. The whole module (dry path included) runs at 2x through a
// linear-phase half-band FIR, so the mix knob never comb-filters and nonlinearities don't alias.
class ColorModule : public Module
{
public:
    enum Type { Warm = 0, Fuzz, Crush, Fold, Sweep, Squash, kNumTypes };

    void prepare (double sampleRate, int maxBlock) override
    {
        os.initProcessing ((size_t) maxBlock);
        os.reset();
        const double r = sampleRate * 2.0;
        prepareBase (r, maxBlock * 2);
        for (int c = 0; c < 2; ++c)
        {
            warmLP[c].setLowpass (900.0f, r); fuzzHP[c].set (140.0f, r); fuzzEnv[c].set (1.0f, 60.0f, r);
            sweepEnv[c].set (3.0f, 140.0f, r);
            dc[c] = {}; toneF[c].reset(); sweepF[c].reset(); crushHold[c] = 0; crushAcc[c] = 0; gate[c] = 1;
        }
        compEnv = 0; compGain = 1;
        for (int t = 0; t < kNumTypes; ++t) resetType (t);
    }

    float latency() const override { return (float) os.getLatencyInSamples(); }

    void process (float* L, float* R, int n, const Ctx& c) override
    {
        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
        auto up = os.processSamplesUp (blk);
        run (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples(), c);
        os.processSamplesDown (blk);
    }

    // Static transfer shapes, also used by the UI screen
    static float shape (int type, float x, float amount, float tweak)
    {
        switch (type)
        {
            case Warm:  { float d = dbToGain (amount * 32.0f), b = (tweak - 0.5f) * 0.8f; return (fastTanh (x * d + b) - fastTanh (b)) * std::pow (d, -0.35f); }
            case Fuzz:  { float g = dbToGain (12.0f + amount * 42.0f); float v = x * g; float y = fastTanh (v + 0.15f) - fastTanh (0.15f); return juce::jlimit (-1.0f, 1.0f, y * 1.25f) * 0.8f; }
            case Crush: { float q = std::pow (2.0f, 15.0f - amount * 13.0f); return std::round (x * q) / q; }
            case Fold:  { float g = 1.0f + amount * 7.0f, s = (tweak - 0.5f) * 1.2f; return (std::sin ((x * g + s) * kPi * 0.5f) - std::sin (s * kPi * 0.5f)) * 0.75f; }
            default:    return x;
        }
    }

protected:
    void resetType (int t) override
    {
        for (int c = 0; c < 2; ++c)
        {
            if (t == Sweep) sweepF[c].reset();
            if (t == Crush) { crushHold[c] = 0; crushAcc[c] = 0; }
            toneF[c].reset();
        }
        if (t == Squash) { compEnv = 0; compGain = 1; }
    }

    void processType (int type, float* L, float* R, int n) override
    {
        float* ch[2] = { L, R };
        switch (type)
        {
            case Warm:
            {
                const float d = dbToGain (amt * 32.0f), b = (tw - 0.5f) * 0.8f, comp = std::pow (d, -0.55f), ob = fastTanh (b);
                const float lowG = 1.0f + (0.5f - tn) * 0.9f, highG = juce::jmax (0.15f, 1.0f + (tn - 0.5f) * 1.6f);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        float y = (fastTanh (ch[c][i] * d + b) - ob) * comp;
                        y = dc[c].process (y);
                        const float lo = warmLP[c].lp (y);
                        ch[c][i] = lo * lowG + (y - lo) * highG;
                    }
                break;
            }
            case Fuzz:
            {
                const float g = dbToGain (12.0f + amt * 42.0f), thr = tw * tw * 0.06f + 1.0e-5f;
                for (int c = 0; c < 2; ++c)
                {
                    toneF[c].set (600.0f * std::pow (2.0f, tn * 5.0f), 0.75f, sr);
                    for (int i = 0; i < n; ++i)
                    {
                        const float x = ch[c][i];
                        const float e = fuzzEnv[c].process (x);
                        const float gt = juce::jlimit (0.0f, 1.0f, e / thr);
                        gate[c] += 0.002f * (gt - gate[c]);
                        float v = fuzzHP[c].process (x) * g;
                        float y = fastTanh (v + 0.15f) - fastTanh (0.15f);
                        y = juce::jlimit (-1.0f, 1.0f, y * 1.25f);
                        ch[c][i] = toneF[c].lp (dc[c].process (y)) * 0.17f * gate[c];
                    }
                }
                break;
            }
            case Crush:
            {
                const float q = std::pow (2.0f, 15.0f - amt * 13.0f), hold = 1.0f + tw * tw * 48.0f;
                for (int c = 0; c < 2; ++c)
                {
                    toneF[c].set (900.0f * std::pow (2.0f, tn * 4.4f), 0.7f, sr);
                    for (int i = 0; i < n; ++i)
                    {
                        crushAcc[c] += 1.0f;
                        if (crushAcc[c] >= hold) { crushAcc[c] -= hold; crushHold[c] = std::round (ch[c][i] * q) / q; }
                        ch[c][i] = toneF[c].lp (crushHold[c]);
                    }
                }
                break;
            }
            case Fold:
            {
                const float g = 1.0f + amt * 7.0f, s = (tw - 0.5f) * 1.2f, os0 = std::sin (s * kPi * 0.5f), comp = 0.75f * std::pow (g, -0.55f);
                for (int c = 0; c < 2; ++c)
                {
                    toneF[c].set (700.0f * std::pow (2.0f, tn * 4.8f), 0.7f, sr);
                    for (int i = 0; i < n; ++i)
                        ch[c][i] = toneF[c].lp (dc[c].process ((std::sin ((ch[c][i] * g + s) * kPi * 0.5f) - os0) * comp));
                }
                break;
            }
            case Sweep:
            {
                const float base = 140.0f * std::pow (2.0f, tw * 6.6f), q = 0.7f + tn * 11.0f;
                float shown = 0.0f;
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < n; ++i)
                    {
                        const float x = ch[c][i];
                        const float e = sweepEnv[c].process (x);
                        const float fc = base * std::pow (2.0f, juce::jlimit (0.0f, 5.0f, e * amt * 28.0f));
                        sweepF[c].set (fc, q, sr);
                        float lp, bp, hp; sweepF[c].process (x, lp, bp, hp);
                        ch[c][i] = lp * 0.55f + bp * 0.9f / std::sqrt (q);
                        shown = fc;
                    }
                uiA.store (shown);
                break;
            }
            case Squash:
            {
                // feed-forward, stereo-linked, soft-knee compressor with program-dependent release
                const float thr = -amt * 42.0f, ratio = 2.0f + tw * 18.0f, knee = 6.0f;
                const float atk = 1.0f - std::exp (-1.0f / (0.003f * (float) sr));
                const float rel = 1.0f - std::exp (-1.0f / ((0.04f + tn * 0.76f) * (float) sr));
                const float makeup = dbToGain (-thr * (1.0f - 1.0f / ratio) * 0.55f);
                float grMax = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float pk = juce::jmax (std::abs (L[i]), std::abs (R[i]));
                    compEnv += (pk > compEnv ? atk : rel) * (pk - compEnv);
                    const float lv = juce::Decibels::gainToDecibels (compEnv + 1.0e-9f, -120.0f);
                    const float over = lv - thr;
                    float grDb = 0.0f;
                    if (over > knee * 0.5f)       grDb = over * (1.0f - 1.0f / ratio);
                    else if (over > -knee * 0.5f) { const float t = over + knee * 0.5f; grDb = (1.0f - 1.0f / ratio) * t * t / (2.0f * knee); }
                    const float target = dbToGain (-grDb);
                    compGain += (target < compGain ? atk : rel) * (target - compGain);
                    const float g = compGain * makeup;
                    L[i] = fastTanh (L[i] * g * 0.9f) / 0.9f; R[i] = fastTanh (R[i] * g * 0.9f) / 0.9f;
                    grMax = juce::jmax (grMax, 1.0f - compGain);
                }
                uiB.store (grMax);
                break;
            }
            default: break;
        }
    }

private:
    juce::dsp::Oversampling<float> os { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    OnePole warmLP[2]; OnePoleHP fuzzHP[2]; EnvFollower fuzzEnv[2], sweepEnv[2];
    SVF toneF[2], sweepF[2]; DCBlock dc[2];
    float crushHold[2] {}, crushAcc[2] {}, gate[2] { 1, 1 };
    float compEnv = 0, compGain = 1;
};
} // namespace prism
