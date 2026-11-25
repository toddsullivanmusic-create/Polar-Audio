#pragma once
#include "Module.h"

namespace prism
{
// SPACE — delays. Delay time glides like tape (no clicks when you turn Time), the feedback path is
// saturated and tone-filtered, and Sync quantises Time to musical divisions of the host tempo.
class SpaceModule : public Module
{
public:
    enum Type { Echo = 0, Ping, Reverse, Shimmer, Multi, Grain, kNumTypes };

    void prepare (double sampleRate, int maxBlock) override
    {
        prepareBase (sampleRate, maxBlock);
        trails = true;
        const int maxD = (int) (sampleRate * 4.2);
        for (int t = 0; t < kNumTypes; ++t) for (int c = 0; c < 2; ++c) dl[t][c].prepare (t == Grain ? (int) (sampleRate * 3.0) : maxD);
        for (int c = 0; c < 2; ++c) shimmerPS[c].prepare (sampleRate, 70.0f);
        for (int t = 0; t < kNumTypes; ++t) { resetType (t); dS[t] = (float) (0.35 * sampleRate); }
    }

    // Delay time in seconds for the current tweak (free or synced)
    static float timeSeconds (float tw, bool sync, double bpm)
    {
        if (sync) { int n; auto* d = divisions (n); return d[divisionIndex (tw)].beats * 60.0f / (float) juce::jmax (30.0, bpm); }
        return 0.03f * std::pow (2.0f, tw * 5.64f);     // 30 ms .. 1.5 s
    }

protected:
    void resetType (int t) override
    {
        for (int c = 0; c < 2; ++c) { dl[t][c].clear(); fbLP[t][c].z = 0; fbHP[t][c].lpf.z = 0; last[t][c] = 0; }
        if (t == Shimmer) for (auto& p : shimmerPS) p.clear();
        if (t == Reverse) revPhase = 0;
        if (t == Grain) for (auto& g : grains) g.active = false;
    }

    // tone < 0.5 darkens the repeats, > 0.5 thins them out
    inline float loopFilter (int t, int c, float x)
    {
        return fbHP[t][c].process (fbLP[t][c].lp (x));
    }
    void setLoopFilter (int t)
    {
        const float lp = tn < 0.5f ? 1200.0f * std::pow (2.0f, tn * 2.0f * 4.0f) : 18000.0f;
        const float hp = tn > 0.5f ? 20.0f * std::pow (2.0f, (tn - 0.5f) * 2.0f * 5.3f) : 20.0f;
        for (int c = 0; c < 2; ++c) { fbLP[t][c].setLowpass (lp, sr); fbHP[t][c].set (hp, sr); }
    }

    void processType (int type, float* L, float* R, int n) override
    {
        const bool frz = ctx->freeze;
        const float inG = frz ? 0.0f : 1.0f;
        const float fb = frz ? 1.0f : amt * 0.97f;
        const float target = timeSeconds (tw, ctx->sync, ctx->bpm) * (float) sr;
        const float glide = 1.0f - std::exp (-1.0f / (0.06f * (float) sr));
        setLoopFilter (type);
        auto& d = dS[type];
        auto* D = dl[type];

        switch (type)
        {
            case Echo:
            {
                for (int i = 0; i < n; ++i)
                {
                    d += glide * (target - d);
                    wow += 0.9f / (float) sr; if (wow >= 1.0f) wow -= 1.0f;
                    flut += 6.3f / (float) sr; if (flut >= 1.0f) flut -= 1.0f;
                    const float mod = 1.0f + 0.0012f * std::sin (wow * 2.0f * kPi) + 0.0003f * std::sin (flut * 2.0f * kPi);
                    const float ol = D[0].read (d * mod), orr = D[1].read (d * mod * 1.007f);
                    D[0].push (L[i] * inG + fastTanh (loopFilter (type, 0, ol) * fb));
                    D[1].push (R[i] * inG + fastTanh (loopFilter (type, 1, orr) * fb));
                    L[i] = ol; R[i] = orr;
                }
                break;
            }
            case Ping:
            {
                for (int i = 0; i < n; ++i)
                {
                    d += glide * (target - d);
                    const float ol = D[0].read (d), orr = D[1].read (d);
                    const float mono = (L[i] + R[i]) * 0.5f * inG;
                    D[0].push (mono + fastTanh (loopFilter (type, 0, orr) * fb));
                    D[1].push (fastTanh (loopFilter (type, 1, ol) * fb));
                    L[i] = ol; R[i] = orr;
                }
                break;
            }
            case Reverse:
            {
                // two overlapping reversed grains, each one delay-time long, sin^2 crossfaded
                for (int i = 0; i < n; ++i)
                {
                    d += glide * (juce::jlimit ((float) sr * 0.06f, (float) sr * 1.9f, target) - d);
                    revPhase += 1.0f / d; if (revPhase >= 1.0f) revPhase -= 1.0f;
                    float p2 = revPhase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
                    const float g1 = std::sin (kPi * revPhase), g2 = std::sin (kPi * p2);
                    float out[2];
                    for (int c = 0; c < 2; ++c)
                        out[c] = D[c].read (3.0f + revPhase * 2.0f * d) * g1 * g1 + D[c].read (3.0f + p2 * 2.0f * d) * g2 * g2;
                    D[0].push (L[i] * inG + loopFilter (type, 0, out[0]) * fb * 0.85f);
                    D[1].push (R[i] * inG + loopFilter (type, 1, out[1]) * fb * 0.85f);
                    L[i] = out[0]; R[i] = out[1];
                }
                break;
            }
            case Shimmer:
            {
                for (int i = 0; i < n; ++i)
                {
                    d += glide * (target - d);
                    const float ol = D[0].read (d), orr = D[1].read (d * 1.013f);
                    const float sl = shimmerPS[0].process (ol, 2.0f), sh = shimmerPS[1].process (orr, 2.0f);
                    D[0].push (L[i] * inG + fastTanh (loopFilter (type, 0, ol * 0.45f + sl * 0.6f) * fb));
                    D[1].push (R[i] * inG + fastTanh (loopFilter (type, 1, orr * 0.45f + sh * 0.6f) * fb));
                    L[i] = ol; R[i] = orr;
                }
                break;
            }
            case Multi:
            {
                // four rhythmic taps across the time window, alternating sides; feedback from the last tap
                static constexpr float tg[4] = { 0.55f, 0.85f, 0.4f, 1.0f };
                for (int i = 0; i < n; ++i)
                {
                    d += glide * (target - d);
                    const float t1 = D[0].read (d * 0.25f), t2 = D[0].read (d * 0.5f), t3 = D[0].read (d * 0.75f), t4 = D[0].read (d);
                    const float mono = (L[i] + R[i]) * 0.5f * inG;
                    D[0].push (mono + fastTanh (loopFilter (type, 0, t4) * fb));
                    L[i] = t1 * tg[0] + t3 * tg[2] + t4 * tg[3] * 0.7f;
                    R[i] = t2 * tg[1] + t4 * tg[3] * 0.7f;
                }
                break;
            }
            case Grain:
            {
                // granular cloud: Amount = density, Time knob = grain size, Tone = pitch scatter
                const float density = 3.0f + amt * 45.0f;
                const float size = (float) sr * 0.03f * std::pow (2.0f, tw * 3.4f);
                for (int i = 0; i < n; ++i)
                {
                    const float mono = (L[i] + R[i]) * 0.5f * inG;
                    if (noise.uni() < density / (float) sr) spawnGrain (size);
                    float ol = 0, orr = 0;
                    for (auto& g : grains)
                    {
                        if (! g.active) continue;
                        const float ph = g.age / g.len;
                        const float w = 0.5f - 0.5f * std::cos (2.0f * kPi * ph);
                        const float s = D[0].read (g.delay) * w;
                        ol += s * g.gl; orr += s * g.gr;
                        g.delay += 1.0f - g.speed; g.age += 1.0f;
                        if (g.age >= g.len) g.active = false;
                    }
                    ol *= 0.5f; orr *= 0.5f;
                    D[0].push (mono + loopFilter (type, 0, (ol + orr) * 0.5f) * (frz ? 0.98f : 0.25f));
                    L[i] = ol; R[i] = orr;
                }
                break;
            }
            default: break;
        }
        uiA.store (d / (float) sr);
    }

    void spawnGrain (float size)
    {
        for (auto& g : grains)
        {
            if (g.active) continue;
            float speed = 1.0f;
            if (tn > 0.66f) { const float r = noise.uni(); speed = r < 0.33f ? 0.5f : (r < 0.66f ? 1.0f : 2.0f); }
            else if (tn > 0.15f) speed = std::pow (2.0f, noise.next() * (tn - 0.15f) * 0.5f);
            g.len = size * (0.7f + 0.6f * noise.uni());
            const float need = g.len * juce::jmax (0.0f, speed - 1.0f) + 4.0f;
            g.delay = need + noise.uni() * (float) sr * 1.2f;
            g.speed = speed; g.age = 0;
            const float pan = noise.uni();
            g.gl = std::cos (pan * kPi * 0.5f); g.gr = std::sin (pan * kPi * 0.5f);
            g.active = true;
            return;
        }
    }

private:
    struct GrainVoice { bool active = false; float delay = 0, speed = 1, len = 1, age = 0, gl = 0.7f, gr = 0.7f; };
    HDelay dl[kNumTypes][2];
    OnePole fbLP[kNumTypes][2]; OnePoleHP fbHP[kNumTypes][2];
    float last[kNumTypes][2] {}, dS[kNumTypes] {};
    PitchShifter shimmerPS[2];
    std::array<GrainVoice, 24> grains;
    float revPhase = 0, wow = 0, flut = 0;
    Noise noise;
};
} // namespace prism
