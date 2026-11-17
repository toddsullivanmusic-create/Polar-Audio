#pragma once
#include "DSP/Utils.h"

namespace glow
{
using namespace lumen::dsp;

// GLOW core. Signal path (per channel, inside 4x oversampling):
//   3-band Linkwitz-Riley split -> per-band drive (Focus) -> circuit waveshaper -> recombine
// Circuits are modelled as memoryless nonlinearities with circuit-specific pre/post filtering
// (tape pre-emphasis, transformer LF weighting, tube bias) rather than a single generic tanh.
class SaturationEngine
{
public:
    enum Circuit { Tube = 0, Tape, Transformer, Crunch };

    struct Params { float driveDb = 6, bias = 0.2f, focus = 0.0f, dynamics = 0.0f; int circuit = Tube; };

    void prepare (double oversampledRate, int numChannels)
    {
        sr = oversampledRate;
        for (int ch = 0; ch < 2; ++ch)
        {
            lowSplit[ch].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            lowSplitHP[ch].setType (juce::dsp::LinkwitzRileyFilterType::highpass);
            highSplit[ch].setType (juce::dsp::LinkwitzRileyFilterType::highpass);
            highSplitLP[ch].setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            juce::dsp::ProcessSpec spec { sr, 512, 1 };
            for (auto* f : { &lowSplit[ch], &lowSplitHP[ch], &highSplit[ch], &highSplitLP[ch] }) { f->prepare (spec); f->reset(); }
            lowSplit[ch].setCutoffFrequency (160.0f); lowSplitHP[ch].setCutoffFrequency (160.0f);
            highSplit[ch].setCutoffFrequency (3200.0f); highSplitLP[ch].setCutoffFrequency (3200.0f);
            tapePre[ch].setLowpass (4500.0f, sr); tapePost[ch].setLowpass (12000.0f, sr);
            xfmrLP[ch].setLowpass (120.0f, sr);
            env[ch].setLowpass (12.0f, sr);
            dc[ch] = {};
        }
        juce::ignoreUnused (numChannels);
    }

    void setParams (const Params& p) { params = p; drive = juce::Decibels::decibelsToGain (p.driveDb); }

    // Transfer function used by the UI for the curve display (static part only).
    static float shape (float x, int circuit, float bias)
    {
        switch (circuit)
        {
            case Tube:        { float b = bias * 0.6f; return fastTanh (x + b) - fastTanh (b); }
            case Tape:        { return fastTanh (x * 0.9f) * 1.05f + 0.02f * x; }
            case Transformer: { float y = x - 0.18f * x * x * x; return std::abs (x) < 1.6f ? y : (x > 0 ? 1.0f : -1.0f) * (1.0f + 0.13f * fastTanh (std::abs (x) - 1.6f)); }
            default:          { float k = 2.2f; float y = x * k; return std::abs (y) < 0.7f ? y : (y > 0 ? 1 : -1) * (0.7f + 0.3f * fastTanh ((std::abs (y) - 0.7f) / 0.3f)); }
        }
    }

    inline float processSample (float x, int ch)
    {
        // envelope for the Dynamics control (drive rides the signal level)
        float e = env[ch].lp (std::abs (x));
        float d = drive * (1.0f + params.dynamics * juce::jlimit (0.0f, 2.0f, e * 4.0f));

        // 3-band split
        float low  = lowSplit[ch].processSample (0, x);
        float rest = lowSplitHP[ch].processSample (0, x);
        float high = highSplit[ch].processSample (0, rest);
        float mid  = highSplitLP[ch].processSample (0, rest);

        float f = params.focus;
        float lowD = d * (f < 0 ? 1.0f + (-f) * 1.2f : 1.0f - f * 0.75f);
        float midD = d;
        float highD = d * (f > 0 ? 1.0f + f * 1.2f : 1.0f - (-f) * 0.75f);

        float y = sat (low * lowD, ch, 0) / norm (lowD) + sat (mid * midD, ch, 1) / norm (midD) + sat (high * highD, ch, 2) / norm (highD);
        return dc[ch].process (y);
    }

private:
    inline float norm (float g) const { return juce::jmax (1.0f, std::pow (g, 0.6f)); } // keeps perceived level roughly constant pre auto-gain

    inline float sat (float x, int ch, int band)
    {
        switch (params.circuit)
        {
            case Tube: return shape (x, Tube, params.bias);
            case Tape:
            {
                // pre-emphasis boosts HF into the tanh so loud HF compresses first (tape "softening")
                float pre = band == 2 ? x + 0.6f * (x - tapePre[ch].lp (x)) : x;
                float y = shape (pre, Tape, 0);
                return band == 2 ? tapePost[ch].lp (y) : y;
            }
            case Transformer:
            {
                // core saturation is LF-weighted: lows hit the iron hardest, highs stay clean
                float y = shape (x * (band == 0 ? 1.4f : band == 1 ? 0.8f : 0.5f), Transformer, 0);
                return band == 0 ? y + 0.1f * xfmrLP[ch].lp (y) : y;
            }
            default: return shape (x, Crunch, 0) * 0.9f;
        }
    }

    double sr = 44100;
    Params params; float drive = 1.0f;
    juce::dsp::LinkwitzRileyFilter<float> lowSplit[2], lowSplitHP[2], highSplit[2], highSplitLP[2];
    OnePole tapePre[2], tapePost[2], xfmrLP[2], env[2];
    DCBlock dc[2];
};
} // namespace glow
