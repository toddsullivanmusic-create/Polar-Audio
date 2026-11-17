#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace polar::dsp
{
constexpr float kPi = 3.14159265358979f;

inline float fastTanh (float x)
{
    // Padé approximant, accurate to ~1e-4 in [-3,3], clamps beyond
    if (x < -3.0f) return -1.0f;
    if (x >  3.0f) return  1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float softClip (float x) { return x - (x * x * x) / 3.0f; } // for |x|<=1.5 region

// One-pole smoother for parameters (per-sample).
struct Smooth
{
    float z = 0, a = 0.001f;
    void setTime (float seconds, double sr) { a = 1.0f - std::exp (-1.0f / (float) (seconds * sr)); }
    void reset (float v) { z = v; }
    inline float process (float target) { z += a * (target - z); return z; }
};

// Simple one-pole lowpass/highpass used in loops where juce::dsp::IIR is too heavy.
struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setLowpass (float hz, double sr) { a = 1.0f - std::exp (-2.0f * kPi * hz / (float) sr); }
    inline float lp (float x) { z += a * (x - z); return z; }
    inline float hp (float x) { return x - lp (x); }
};

// DC blocker
struct DCBlock
{
    float x1 = 0, y1 = 0, R = 0.995f;
    inline float process (float x) { float y = x - x1 + R * y1; x1 = x; y1 = y; return y; }
};

// Delay line with linear interpolation, power-of-two length
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        size = 1; while (size < maxSamples + 2) size <<= 1;
        mask = size - 1; buf.assign ((size_t) size, 0.0f); w = 0;
    }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (float delaySamples) const
    {
        float rp = (float) w - delaySamples; while (rp < 0) rp += (float) size;
        int i0 = (int) rp; float f = rp - (float) i0;
        int i1 = (i0 + 1) & mask; i0 &= mask;
        return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
    }
    inline float readInt (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
private:
    std::vector<float> buf; int size = 0, mask = 0, w = 0;
};

// Schroeder allpass with modulation support
struct Allpass
{
    DelayLine dl; float g = 0.7f; float len = 100.0f;
    void prepare (int maxLen) { dl.prepare (maxLen); }
    inline float process (float x, float delay)
    {
        float d = dl.read (delay);
        float v = x - g * d;
        dl.push (v);
        return d + g * v;
    }
};

// Lightweight LFO
struct LFO
{
    float phase = 0, inc = 0;
    void setRate (float hz, double sr) { inc = hz / (float) sr; }
    inline float sine() { phase += inc; if (phase >= 1.0f) phase -= 1.0f; return std::sin (phase * 2.0f * kPi); }
    inline float tri()  { phase += inc; if (phase >= 1.0f) phase -= 1.0f; return 1.0f - 4.0f * std::abs (phase - 0.5f); }
};

// Peak hold helper for meters
inline void trackPeak (std::atomic<float>& dst, const float* data, int n)
{
    float p = 0; for (int i = 0; i < n; ++i) p = std::max (p, std::abs (data[i]));
    float cur = dst.load(); while (p > cur && ! dst.compare_exchange_weak (cur, p)) {}
}
} // namespace polar::dsp
