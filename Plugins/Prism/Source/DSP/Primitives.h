#pragma once
#include "DSP/Utils.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

// High-fidelity building blocks for PRISM. Everything here is allocation-free after prepare().
namespace prism
{
using namespace polar::dsp;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float clamp01 (float x)   { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// ---------------------------------------------------------------------------------------------
// Delay line with 4-point, 3rd-order Hermite interpolation (much cleaner than linear when modulated)
// ---------------------------------------------------------------------------------------------
struct HDelay
{
    std::vector<float> buf; int mask = 0, w = 0, size = 0;

    void prepare (int maxSamples)
    {
        size = 1; while (size < maxSamples + 8) size <<= 1;
        mask = size - 1; buf.assign ((size_t) size, 0.0f); w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float maxDelay() const { return (float) (size - 8); }

    inline float read (float d) const
    {
        d = juce::jlimit (2.01f, (float) (size - 8), d);
        const float rp = (float) w - d;
        const int   i  = (int) std::floor (rp);
        const float f  = rp - (float) i;
        const float xm1 = buf[(size_t) ((i - 1) & mask)], x0 = buf[(size_t) (i & mask)];
        const float x1  = buf[(size_t) ((i + 1) & mask)], x2 = buf[(size_t) ((i + 2) & mask)];
        const float c = (x1 - xm1) * 0.5f, v = x0 - x1, ww = c + v;
        const float a = ww + v + (x2 - x0) * 0.5f, b = ww + a;
        return ((a * f - b) * f + c) * f + x0;
    }
    inline float readAbs (int idx) const { return buf[(size_t) (idx & mask)]; }
    inline int writePos() const { return w; }
};

// ---------------------------------------------------------------------------------------------
// Zero-delay-feedback state-variable filter (Cytomic / Simper TPT form): stable under fast modulation
// ---------------------------------------------------------------------------------------------
struct SVF
{
    float ic1 = 0, ic2 = 0, a1 = 1, a2 = 0, a3 = 0, k = 1.4f;
    void reset() { ic1 = ic2 = 0; }
    inline void set (float fc, float q, double sr)
    {
        fc = juce::jlimit (10.0f, (float) sr * 0.49f, fc);
        const float g = std::tan (kPi * fc / (float) sr);
        k = 1.0f / juce::jmax (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void process (float v0, float& lp, float& bp, float& hp)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    inline float lp (float x) { float l, b, h; process (x, l, b, h); return l; }
};

// ---------------------------------------------------------------------------------------------
// Envelope follower with separate attack / release
// ---------------------------------------------------------------------------------------------
struct EnvFollower
{
    float env = 0, atk = 0.01f, rel = 0.001f;
    void set (float attackMs, float releaseMs, double sr)
    {
        atk = 1.0f - std::exp (-1.0f / (attackMs * 0.001f * (float) sr));
        rel = 1.0f - std::exp (-1.0f / (releaseMs * 0.001f * (float) sr));
    }
    inline float process (float x) { x = std::abs (x); env += (x > env ? atk : rel) * (x - env); return env; }
};

// ---------------------------------------------------------------------------------------------
// Two-tap granular pitch shifter (sin^2 crossfaded taps; Hermite reads)
// ---------------------------------------------------------------------------------------------
struct PitchShifter
{
    HDelay d; float phase = 0, win = 1000;
    void prepare (double sr, float windowMs) { d.prepare ((int) (sr * 0.25)); win = windowMs * 0.001f * (float) sr; phase = 0; }
    void clear() { d.clear(); phase = 0; }
    inline float process (float x, float ratio)
    {
        d.push (x);
        phase += (1.0f - ratio) / win;
        while (phase >= 1.0f) phase -= 1.0f;
        while (phase < 0.0f)  phase += 1.0f;
        float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        const float s1 = std::sin (kPi * phase), s2 = std::sin (kPi * p2);
        return d.read (3.0f + phase * win) * s1 * s1 + d.read (3.0f + p2 * win) * s2 * s2;
    }
};

// One-pole HP for loops
struct OnePoleHP
{
    OnePole lpf;
    void set (float hz, double sr) { lpf.setLowpass (hz, sr); }
    inline float process (float x) { return x - lpf.lp (x); }
};

// Fast xorshift noise in [-1, 1]
struct Noise
{
    juce::uint32 s = 0x9E3779B9u;
    inline float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xFFFFFF) / 8388608.0f - 1.0f; }
    inline float uni() { return 0.5f * (next() + 1.0f); }
};

// Tempo divisions used by synced controls (in beats)
struct Division { const char* name; float beats; };
inline const Division* divisions (int& count)
{
    static const Division d[] = {
        { "1/32", 0.125f }, { "1/16T", 1.0f / 6.0f }, { "1/16", 0.25f }, { "1/8T", 1.0f / 3.0f }, { "1/16D", 0.375f },
        { "1/8", 0.5f }, { "1/4T", 2.0f / 3.0f }, { "1/8D", 0.75f }, { "1/4", 1.0f }, { "1/2T", 4.0f / 3.0f },
        { "1/4D", 1.5f }, { "1/2", 2.0f }, { "1 bar", 4.0f } };
    count = (int) (sizeof (d) / sizeof (d[0]));
    return d;
}
inline int divisionIndex (float knob01) { int n; divisions (n); return juce::jlimit (0, n - 1, (int) (knob01 * (float) n)); }
} // namespace prism
