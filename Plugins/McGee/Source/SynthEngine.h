#pragma once
#include "DSP/Utils.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace mcgee
{
using namespace lumen::dsp;

// Shared per-instance parameters (read by every voice)
struct SynthParams
{
    int   osc1Wave = 0, osc2Wave = 0, osc3Wave = 2;        // 0 saw, 1 square, 2 tri, 3 sine
    float osc1Lvl = 1, osc2Lvl = 0.8f, osc3Lvl = 0.5f;
    int   osc2Oct = 0, osc3Oct = -1;                      // octave offsets
    float osc2Detune = 7, osc3Detune = -5;               // cents
    float pw = 0.5f;                                      // pulse width
    float fat = 0.3f;                                     // unison spread (2 extra detuned copies of osc1)
    float cutoff = 2000, reso = 0.3f, envAmt = 0.5f, keyTrack = 0.5f, filterDrive = 0.2f;
    float fA = 0.01f, fD = 0.4f, fS = 0.3f, fR = 0.5f;
    float aA = 0.005f, aD = 0.3f, aS = 0.8f, aR = 0.4f;
    float lfoRate = 4, lfoToPitch = 0, lfoToCutoff = 0;
    float glide = 0; bool mono = false;
    float drive = 0;                                      // output drive
};

// PolyBLEP oscillator — alias-suppressed saw/square/tri
struct Oscillator
{
    float phase = 0, inc = 0;
    void setFreq (float hz, double sr) { inc = hz / (float) sr; }
    static inline float blep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
    inline float next (int wave, float pw, float& triState)
    {
        phase += inc; if (phase >= 1.0f) phase -= 1.0f;
        float t = phase, dt = juce::jmax (1e-5f, inc);
        switch (wave)
        {
            case 0: { float v = 2.0f * t - 1.0f; return v - blep (t, dt); }
            case 1:
            {
                float v = t < pw ? 1.0f : -1.0f;
                v += blep (t, dt);
                float t2 = t + 1.0f - pw; if (t2 >= 1.0f) t2 -= 1.0f;
                v -= blep (t2, dt);
                return v;
            }
            case 2: { juce::ignoreUnused (triState); return 1.0f - 4.0f * std::abs (t - 0.5f); } // triangle harmonics fall at 1/n^2: aliasing negligible
            default: return std::sin (t * 2.0f * kPi);
        }
    }
};

struct ADSR
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle; float value = 0; double sr = 44100;
    float a = 0.01f, d = 0.3f, s = 0.8f, r = 0.4f;
    void noteOn()  { stage = Attack; }
    void noteOff() { if (stage != Idle) stage = Release; }
    inline float next()
    {
        auto coef = [this] (float secs) { return 1.0f - std::exp (-1.0f / (juce::jmax (0.0005f, secs) * (float) sr)); };
        switch (stage)
        {
            case Attack:  value += coef (a) * (1.2f - value); if (value >= 1.0f) { value = 1.0f; stage = Decay; } break;
            case Decay:   value += coef (d) * (s - value); if (std::abs (value - s) < 0.001f) stage = Sustain; break;
            case Sustain: value = s; break;
            case Release: value += coef (r * 0.5f) * (0.0f - value); if (value < 0.0005f) { value = 0; stage = Idle; } break;
            default: break;
        }
        return value;
    }
};

// Moog-style 4-pole ladder (Huovilainen-inspired, with tanh nonlinearities and 2x internal oversampling)
struct Ladder
{
    double sr = 44100; float s[4] {}, delay[4] {}; float g = 0, k = 0;
    void prepare (double rate) { sr = rate; reset(); }
    void reset() { for (auto& v : s) v = 0; for (auto& v : delay) v = 0; }
    void set (float cutoffHz, float resonance)
    {
        float fc = juce::jlimit (20.0f, (float) sr * 0.45f, cutoffHz) / (float) (sr * 2.0);
        g = 1.0f - std::exp (-2.0f * kPi * fc);
        k = 4.0f * juce::jlimit (0.0f, 1.0f, resonance) * (1.0f - 0.15f * fc * 4.0f);
    }
    inline float process (float x, float drive)
    {
        float out = 0;
        for (int os = 0; os < 2; ++os)
        {
            float in = fastTanh ((x - k * s[3]) * (1.0f + drive * 2.0f)) / (1.0f + drive);
            s[0] += g * (in - s[0]);
            s[1] += g * (fastTanh (s[0]) - s[1]);
            s[2] += g * (fastTanh (s[1]) - s[2]);
            s[3] += g * (fastTanh (s[2]) - s[3]);
            out = s[3];
        }
        return out * (1.0f + k * 0.35f); // resonance level compensation
    }
};

class Voice
{
public:
    void prepare (double rate) { sr = rate; ladder.prepare (rate); fEnv.sr = rate; aEnv.sr = rate; lfo.setRate (4, rate); }

    void noteOn (int midiNote, float velocity, const SynthParams& p, float glideFrom)
    {
        note = midiNote; vel = velocity; active = true;
        targetFreq = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
        curFreq = (p.glide > 0.001f && glideFrom > 0) ? glideFrom : targetFreq;
        fEnv = { }; aEnv = { }; fEnv.sr = sr; aEnv.sr = sr;
        fEnv.a = p.fA; fEnv.d = p.fD; fEnv.s = p.fS; fEnv.r = p.fR;
        aEnv.a = p.aA; aEnv.d = p.aD; aEnv.s = p.aS; aEnv.r = p.aR;
        fEnv.noteOn(); aEnv.noteOn();
        juce::Random rnd; for (auto& o : osc) o.phase = rnd.nextFloat(); // free-running feel
    }
    void noteOff() { fEnv.noteOff(); aEnv.noteOff(); }
    bool isActive() const { return active; }
    int  getNote() const { return note; }
    float currentFreq() const { return curFreq; }

    inline float render (const SynthParams& p, float lfoVal)
    {
        if (! active) return 0.0f;
        // glide
        if (p.glide > 0.001f) curFreq += (targetFreq - curFreq) * (1.0f - std::exp (-1.0f / (p.glide * (float) sr)));
        else curFreq = targetFreq;
        float pitchMod = std::pow (2.0f, lfoVal * p.lfoToPitch * 0.5f / 12.0f);
        float f = curFreq * pitchMod;
        float det2 = std::pow (2.0f, (p.osc2Detune / 1200.0f) + p.osc2Oct);
        float det3 = std::pow (2.0f, (p.osc3Detune / 1200.0f) + p.osc3Oct);
        osc[0].setFreq (f, sr); osc[1].setFreq (f * det2, sr); osc[2].setFreq (f * det3, sr);
        float spread = p.fat * 0.012f;
        osc[3].setFreq (f * (1.0f + spread), sr); osc[4].setFreq (f * (1.0f - spread), sr);

        float x = osc[0].next (p.osc1Wave, p.pw, tri[0]) * p.osc1Lvl
                + osc[1].next (p.osc2Wave, p.pw, tri[1]) * p.osc2Lvl
                + osc[2].next (p.osc3Wave, p.pw, tri[2]) * p.osc3Lvl;
        if (p.fat > 0.001f)
            x += (osc[3].next (p.osc1Wave, p.pw, tri[3]) + osc[4].next (p.osc1Wave, p.pw, tri[4])) * p.osc1Lvl * p.fat * 0.6f;
        x *= 0.35f;

        float fe = fEnv.next();
        float keyT = std::pow (2.0f, (float) (note - 60) / 12.0f * p.keyTrack);
        float cutoff = p.cutoff * keyT * std::pow (2.0f, fe * p.envAmt * 6.0f + lfoVal * p.lfoToCutoff * 3.0f);
        ladder.set (cutoff, p.reso);
        float y = ladder.process (x, p.filterDrive);

        float ae = aEnv.next();
        if (aEnv.stage == ADSR::Idle) active = false;
        return y * ae * (0.4f + 0.6f * vel);
    }

private:
    double sr = 44100; bool active = false; int note = 60; float vel = 1;
    float targetFreq = 440, curFreq = 440;
    Oscillator osc[5]; float tri[5] {};
    Ladder ladder; ADSR fEnv, aEnv; LFO lfo;
};

class SynthEngine
{
public:
    static constexpr int kVoices = 16;
    void prepare (double rate, int maxBlock)
    {
        sr = rate; for (auto& v : voices) v.prepare (rate);
        lfo.setRate (4, rate);
        juce::dsp::ProcessSpec spec { rate, (juce::uint32) maxBlock, 2 };
        chorus.prepare (spec); chorus.setCentreDelay (12.0f); chorus.setDepth (0.25f); chorus.setRate (0.35f); chorus.setFeedback (0.0f);
        delay.prepare (spec); delay.setMaximumDelayInSamples ((int) rate * 2);
        delayFb[0] = delayFb[1] = 0;
        hp[0].setLowpass (25.0f, rate); hp[1].setLowpass (25.0f, rate);
    }

    void setParams (const SynthParams& p, float chorusMix, float delayTime, float delayFbAmt, float delayMix)
    {
        params = p; lfo.setRate (p.lfoRate, sr);
        chorusMixV = chorusMix; delayTimeS = delayTime; delayFbV = delayFbAmt; delayMixV = delayMix;
        chorus.setMix (chorusMix);
    }

    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            float glideFrom = lastFreq;
            Voice* v = nullptr;
            if (params.mono) { v = &voices[0]; for (auto& o : voices) if (&o != v) o.noteOff(); }
            else
            {
                for (auto& o : voices) if (! o.isActive()) { v = &o; break; }
                if (! v) v = &voices[(size_t) (steal++ % kVoices)];
            }
            v->noteOn (m.getNoteNumber(), m.getFloatVelocity(), params, glideFrom);
            lastFreq = (float) juce::MidiMessage::getMidiNoteInHertz (m.getNoteNumber());
            held.push_back (m.getNoteNumber());
        }
        else if (m.isNoteOff() || m.isAllNotesOff())
        {
            held.erase (std::remove (held.begin(), held.end(), m.getNoteNumber()), held.end());
            for (auto& v : voices)
                if (v.isActive() && (v.getNote() == m.getNoteNumber() || m.isAllNotesOff())) v.noteOff();
        }
        else if (m.isPitchWheel()) bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    }

    void render (float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            float lv = lfo.sine();
            float mix = 0;
            for (auto& v : voices) mix += v.render (params, lv);
            mix = fastTanh (mix * (1.0f + params.drive * 3.0f)) * (1.0f / (1.0f + params.drive));
            mix = hp[0].hp (mix);
            L[i] = mix; R[i] = mix;
        }
        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
        if (chorusMixV > 0.001f) chorus.process (juce::dsp::ProcessContextReplacing<float> (blk));
        if (delayMixV > 0.001f)
        {
            float dSamps = juce::jlimit (1.0f, (float) sr * 1.99f, delayTimeS * (float) sr);
            for (int i = 0; i < n; ++i)
            {
                float dl = delay.popSample (0, dSamps), dr = delay.popSample (1, dSamps * 1.5f > (float) sr * 1.99f ? dSamps : dSamps * 1.5f);
                delay.pushSample (0, L[i] + dl * delayFbV);
                delay.pushSample (1, R[i] + dr * delayFbV);
                L[i] += dl * delayMixV; R[i] += dr * delayMixV;
            }
        }
        else { for (int i = 0; i < n; ++i) { delay.pushSample (0, L[i]); delay.pushSample (1, R[i]); } }
    }

    int activeVoices() const { int c = 0; for (auto& v : voices) c += v.isActive(); return c; }

private:
    double sr = 44100; SynthParams params; std::array<Voice, kVoices> voices; int steal = 0; float lastFreq = 0; float bend = 0;
    std::vector<int> held; LFO lfo; OnePole hp[2];
    juce::dsp::Chorus<float> chorus; float chorusMixV = 0;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 96000 * 2 };
    float delayFb[2] {}; float delayTimeS = 0.4f, delayFbV = 0.3f, delayMixV = 0;
};
} // namespace mcgee
