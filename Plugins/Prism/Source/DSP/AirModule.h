#pragma once
#include "Module.h"
#include "ReverbEngine.h"   // AURA's modulated FDN

namespace prism
{
// AIR — space and texture. Hall / Plate / Bloom use AURA's 8-line modulated FDN; Spring is a dispersive
// allpass loop; Cassette is wow, flutter, saturation, hiss and dropouts; Glitch is a tempo-locked
// beat-repeater with pitch-falling repeats.
class AirModule : public Module
{
public:
    enum Type { Hall = 0, Plate, Bloom, Spring, Cassette, Glitch, kNumTypes };

    static bool isCompleteEffect (int t) { return t == Cassette || t == Glitch; }   // output includes the dry

    void prepare (double sampleRate, int maxBlock) override
    {
        prepareBase (sampleRate, maxBlock);
        trails = true;
        for (auto& r : rv) r.prepare (sampleRate, 32);
        rvOut.setSize (2, 32);
        for (int c = 0; c < 2; ++c)
        {
            springD[c].prepare ((int) (sampleRate * 0.12)); tapeD[c].prepare ((int) (sampleRate * 0.06));
            glitchBuf[c].prepare ((int) (sampleRate * 8.0));
            for (auto& a : springAP[c]) a.prepare (8);
        }
        for (int t = 0; t < kNumTypes; ++t) resetType (t);
    }

protected:
    bool isWetOnly (int t) const override { return ! isCompleteEffect (t); }

    void resetType (int t) override
    {
        if (t <= Bloom) rv[t].reset();
        for (int c = 0; c < 2; ++c)
        {
            if (t == Spring) { springD[c].clear(); springFb[c] = 0; springLP[c].z = 0; for (auto& a : springAP[c]) a.dl.clear(); }
            if (t == Cassette) { tapeD[c].clear(); }
        }
        if (t == Glitch) { repeatsLeft = 0; sliceIndex = -1; freeCount = 0; }
    }

    void processType (int type, float* L, float* R, int n) override
    {
        switch (type)
        {
            case Hall: case Plate: case Bloom: reverb (type, L, R, n); break;
            case Spring:   spring (L, R, n); break;
            case Cassette: cassette (L, R, n); break;
            case Glitch:   glitch (L, R, n); break;
            default: break;
        }
    }

    void reverb (int type, float* L, float* R, int n)
    {
        aura::ReverbEngine::Params p;
        p.decaySec = 0.4f * std::pow (2.0f, amt * 5.6f);
        p.freeze = ctx->freeze;
        p.lowcutHz = 90.0f; p.width = 1.0f;
        if (type == Hall)  { p.size = 0.35f + 0.65f * tw; p.damp = 1.0f - tn; p.diffusion = 0.78f; p.modRate = 0.3f; p.modDepth = 0.35f; p.predelayMs = 18; p.colour = aura::ReverbEngine::Modern; }
        if (type == Plate) { p.size = 0.25f + 0.45f * tw; p.damp = 0.85f - 0.8f * tn; p.diffusion = 1.0f; p.modRate = 0.45f; p.modDepth = 0.25f; p.predelayMs = 4; p.colour = aura::ReverbEngine::Plate; }
        if (type == Bloom) { p.size = 0.95f; p.damp = 0.45f; p.diffusion = 0.9f; p.modRate = 0.2f; p.modDepth = 0.55f; p.predelayMs = tw * 240.0f; p.shimmer = tn * 0.7f; p.colour = aura::ReverbEngine::Vintage; p.decaySec *= 1.5f; }
        rv[type].setParams (p);
        float* ol = rvOut.getWritePointer (0); float* orr = rvOut.getWritePointer (1);
        rv[type].process (L, R, ol, orr, n);
        const float trim = type == Plate ? 0.55f : 0.5f;   // FDN output sits ~+5 dB over unity; level-match it
        for (int i = 0; i < n; ++i) { L[i] = ol[i] * trim; R[i] = orr[i] * trim; }
        uiA.store (rv[type].tailEnergy);
    }

    void spring (float* L, float* R, int n)
    {
        const float len = (float) sr * (0.022f + tw * 0.04f), fb = 0.55f + amt * 0.42f;
        for (int c = 0; c < 2; ++c) springLP[c].setLowpass (1800.0f * std::pow (2.0f, tn * 2.5f), sr);
        float* ch[2] = { L, R };
        for (int c = 0; c < 2; ++c)
        {
            const float lc = len * (c ? 1.137f : 1.0f);
            for (int i = 0; i < n; ++i)
            {
                float x = ch[c][i] * 0.6f + springFb[c] * fb;
                for (auto& a : springAP[c]) { a.g = 0.62f; x = a.process (x, 2.0f + (c ? 0.3f : 0.0f)); }  // stretched allpasses = chirp
                springD[c].push (x);
                const float y = springLP[c].lp (springD[c].read (lc));
                springFb[c] = fastTanh (y);
                ch[c][i] = y * 0.7f;
            }
        }
    }

    void cassette (float* L, float* R, int n)
    {
        const float ms = (float) sr * 0.001f;
        const float depth = amt * 3.5f * ms;
        const float bw = 16000.0f * std::pow (0.2f, tw);            // ages from 16k down to ~3k
        const float hiss = tw * tw * 0.006f;
        const float tilt = (tn - 0.5f);
        for (int c = 0; c < 2; ++c) { tapeLP[c].setLowpass (bw, sr); tapeTilt[c].setLowpass (700.0f, sr); }
        for (int i = 0; i < n; ++i)
        {
            wow += 0.55f / (float) sr; if (wow >= 1.0f) wow -= 1.0f;
            flut += 7.1f / (float) sr; if (flut >= 1.0f) flut -= 1.0f;
            walk += (noise.next() * 0.002f); walk *= 0.9999f; walk = juce::jlimit (-1.0f, 1.0f, walk);
            const float mod = 0.65f * std::sin (wow * 2.0f * kPi) + 0.5f * walk + 0.12f * std::sin (flut * 2.0f * kPi);
            const float d = 4.0f * ms + depth * (1.0f + mod);
            if (tw > 0.25f && dropTimer <= 0 && noise.uni() < (tw - 0.25f) * 0.8f / (float) sr)
            { dropTimer = (int) ((0.03f + 0.08f * noise.uni()) * (float) sr); dropDepth = 0.3f + 0.5f * noise.uni(); }
            const float dropT = dropTimer > 0 ? 1.0f - dropDepth : 1.0f;
            if (dropTimer > 0) --dropTimer;
            drop += 0.004f * (dropT - drop);
            float* ch[2] = { L, R };
            const float h = noise.next() * hiss;
            for (int c = 0; c < 2; ++c)
            {
                tapeD[c].push (ch[c][i]);
                float y = tapeD[c].read (d);
                y = fastTanh (y * 1.3f) / 1.3f;
                y = tapeLP[c].lp (y);
                const float lo = tapeTilt[c].lp (y);
                y = lo * (1.0f - tilt * 0.8f) + (y - lo) * (1.0f + tilt * 1.2f);
                ch[c][i] = y * drop + h;
            }
        }
    }

    void glitch (float* L, float* R, int n)
    {
        static constexpr float sliceBeats[] = { 0.125f, 0.25f, 0.5f, 1.0f, 2.0f };
        const float beats = sliceBeats[juce::jlimit (0, 4, (int) (tw * 4.99f))];
        const float spb = (float) (60.0 / juce::jmax (30.0, ctx->bpm) * sr);
        const int sliceLen = juce::jmax (64, (int) (beats * spb));
        const int fade = juce::jmin (sliceLen / 4, (int) (0.003 * sr));
        for (int i = 0; i < n; ++i)
        {
            glitchBuf[0].push (L[i]); glitchBuf[1].push (R[i]);
            // slice boundary: host-locked when playing, free-running otherwise
            bool boundary;
            if (ctx->playing) { const long idx = (long) std::floor (ppqAt (i) / beats); boundary = idx != sliceIndex; sliceIndex = idx; }
            else { boundary = ++freeCount >= sliceLen; if (boundary) freeCount = 0; }
            if (boundary)
            {
                if (repeatsLeft > 0) { --repeatsLeft; readPos = 0; ++repeatNum; active = true; }
                else if (noise.uni() < amt)
                {
                    repeatsLeft = 1 + (int) (noise.uni() * 6.0f); repeatNum = 0; readPos = 0;
                    capStart = glitchBuf[0].writePos() - sliceLen;
                    active = true;
                }
                else active = false;
            }
            if (active)
            {
                const float speed = 1.0f - tn * 0.12f * (float) repeatNum;
                const int ip = (int) readPos;
                if (ip >= sliceLen || speed <= 0.2f) { L[i] = 0; R[i] = 0; }
                else
                {
                    float env = 1.0f;
                    if (ip < fade) env = (float) ip / (float) fade;
                    else if (ip > sliceLen - fade) env = (float) (sliceLen - ip) / (float) fade;
                    L[i] = glitchBuf[0].readAbs (capStart + ip) * env;
                    R[i] = glitchBuf[1].readAbs (capStart + ip) * env;
                }
                readPos += juce::jmax (0.2f, speed);
            }
        }
        uiA.store (active ? 1.0f : 0.0f);
    }

private:
    std::array<aura::ReverbEngine, 3> rv;
    juce::AudioBuffer<float> rvOut;
    HDelay springD[2], tapeD[2], glitchBuf[2];
    Allpass springAP[2][12];
    OnePole springLP[2], tapeLP[2], tapeTilt[2];
    float springFb[2] {};
    float wow = 0, flut = 0, walk = 0, drop = 1, dropDepth = 0; int dropTimer = 0;
    int repeatsLeft = 0, repeatNum = 0, capStart = 0, freeCount = 0; long sliceIndex = -1; float readPos = 0; bool active = false;
    Noise noise;
};
} // namespace prism
