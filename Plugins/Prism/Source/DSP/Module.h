#pragma once
#include "Primitives.h"
#include <atomic>

namespace prism
{
// What the host/processor hands each module for one block
struct Ctx
{
    int   type = 0;
    float amount = 0.5f, tweak = 0.5f, tone = 0.5f, mix = 1.0f;
    bool  on = true, freeze = false, sync = false, playing = false;
    double bpm = 120.0, ppq = 0.0;     // ppq at the first sample of this block
    const float* duck = nullptr;       // per-sample duck gain (trail modules only), or nullptr
};

// Base class for one of the four PRISM modules. Handles everything that makes switching and bypassing
// inaudible:
//  * 32-sample sub-blocks with smoothed Amount/Tweak/Tone
//  * equal-gain 30 ms crossfade between the outgoing and incoming effect type (both are processed)
//  * bypass that ramps the effect's *input*, so delay/reverb tails ring out ("trails") instead of cutting
//  * smoothed Mix
class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double sampleRate, int maxBlock) = 0;
    virtual void process (float* L, float* R, int n, const Ctx& c) { run (L, R, n, c); }
    virtual float latency() const { return 0.0f; }

    std::atomic<float> energy { 0 };   // wet energy for the UI screens
    std::atomic<float> uiA { 0 }, uiB { 0 };   // module-specific UI values (LFO phase, delay time, GR...)

protected:
    void prepareBase (double rate, int maxBlock)
    {
        sr = rate;
        wet.setSize (2, 32); tmp.setSize (2, 32);
        xfLen = juce::jmax (1, (int) (0.03 * sr));
        subCoef = 1.0f - std::exp (-32.0f / (0.025f * (float) sr));
        sampCoef = 1.0f - std::exp (-1.0f / (0.012f * (float) sr));
        curType = -1; xfRemain = 0; silentFor = 0;
        juce::ignoreUnused (maxBlock);
    }

    virtual void processType (int type, float* L, float* R, int n) = 0; // in place, full effect output
    virtual void resetType (int) {}
    // Wet-only effects (delays, reverbs) use the pedal mix law: dry stays at unity up to 50 %, wet reaches
    // full level at 50 %. Complete effects (drive, modulation...) crossfade linearly.
    virtual bool isWetOnly (int) const { return trails; }

    void run (float* L, float* R, int n, const Ctx& c)
    {
        ctx = &c;
        if (curType < 0)
        {
            curType = c.type; amt = c.amount; tw = c.tweak; tn = c.tone; mixS = c.mix; onAmt = c.on ? 1.0f : 0.0f;
        }
        if (c.type != curType)
        {
            prevType = curType; curType = c.type; resetType (curType); xfRemain = xfLen; silentFor = 0;
        }
        const float onT = c.on ? 1.0f : 0.0f;
        if (onT == 0.0f && onAmt < 1.0e-4f && xfRemain == 0 && (! trails || silentFor > (int) (sr * 3.0)))
        {
            onAmt = 0.0f; energy.store (0.0f); return;     // fully bypassed and silent: dry passes untouched
        }

        float e = 0.0f;
        for (int off = 0; off < n; off += 32)
        {
            const int m = juce::jmin (32, n - off);
            blockOffset = off;
            amt += subCoef * (c.amount - amt); tw += subCoef * (c.tweak - tw); tn += subCoef * (c.tone - tn);
            float* l = L + off; float* r = R + off;
            float* wl = wet.getWritePointer (0); float* wr = wet.getWritePointer (1);
            float gains[32];
            for (int i = 0; i < m; ++i)
            {
                onAmt += sampCoef * (onT - onAmt);
                gains[i] = onAmt; wl[i] = l[i] * onAmt; wr[i] = r[i] * onAmt;
            }
            if (xfRemain > 0)
            {
                float* tl = tmp.getWritePointer (0); float* tr = tmp.getWritePointer (1);
                std::copy (wl, wl + m, tl); std::copy (wr, wr + m, tr);
                processType (prevType, tl, tr, m);
                processType (curType, wl, wr, m);
                for (int i = 0; i < m; ++i)
                {
                    const float x = (float) juce::jmax (0, xfRemain) / (float) xfLen;
                    wl[i] = tl[i] * x + wl[i] * (1.0f - x); wr[i] = tr[i] * x + wr[i] * (1.0f - x);
                    if (xfRemain > 0) --xfRemain;
                }
            }
            else processType (curType, wl, wr, m);

            float blockE = 0.0f;
            const bool wetOnly = isWetOnly (curType);
            for (int i = 0; i < m; ++i)
            {
                mixS += sampCoef * (c.mix - mixS);
                const float dryG = wetOnly ? juce::jmin (1.0f, 2.0f * (1.0f - mixS)) : 1.0f - mixS;
                const float wetG = wetOnly ? juce::jmin (1.0f, 2.0f * mixS) : mixS;
                const float dk = c.duck ? c.duck[off + i] : 1.0f;
                float a = wl[i], b = wr[i];
                if (! std::isfinite (a)) a = 0.0f;
                if (! std::isfinite (b)) b = 0.0f;
                const float dryMul = 1.0f - (1.0f - dryG) * gains[i];
                l[i] = l[i] * dryMul + a * wetG * dk;
                r[i] = r[i] * dryMul + b * wetG * dk;
                blockE += a * a + b * b;
            }
            e += blockE;
            silentFor = blockE < 1.0e-10f ? silentFor + m : 0;
        }
        energy.store (std::sqrt (e / (float) juce::jmax (1, 2 * n)));
    }

    // Ppq of the sample at `i` within the current sub-block
    double ppqAt (int i) const { return ctx->ppq + (double) (blockOffset + i) * ctx->bpm / (60.0 * sr); }

    double sr = 44100.0;
    const Ctx* ctx = nullptr;
    float amt = 0, tw = 0, tn = 0;
    bool trails = false;
    int blockOffset = 0;

private:
    juce::AudioBuffer<float> wet, tmp;
    int curType = -1, prevType = 0, xfRemain = 0, xfLen = 1, silentFor = 0;
    float onAmt = 0, mixS = 0, subCoef = 0.1f, sampCoef = 0.01f;
};
} // namespace prism
