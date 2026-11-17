#pragma once
#include "DSP/Utils.h"
#include "ReverbEngine.h"   // AURA core, used here as a tiny dirty gated room
#include <juce_audio_basics/juce_audio_basics.h>

// McGEE — a polysynth built for the "underwater" lo-fi texture: alias-free analog oscillators and a
// transistor-ladder filter, then a TEXTURE stage that does the things the records do to their sounds:
// cassette wow & flutter, 12-bit / sample-rate crunch, muffle, tape saturation, hiss, a slow swirling
// "haze" chorus, a short dirty gated room, and a modulated tape delay.

namespace mcgee
{
using namespace polar::dsp;

struct SynthParams
{
    int   osc1Wave = 0, osc2Wave = 0, osc3Wave = 2;        // 0 saw, 1 square, 2 tri, 3 sine, 4 FM keys
    float osc1Lvl = 1, osc2Lvl = 0.8f, osc3Lvl = 0.5f;
    int   osc2Oct = 0, osc3Oct = -1;
    float osc2Detune = 7, osc3Detune = -5;               // cents
    float pw = 0.5f, fat = 0.3f, fm = 0.3f, fmRatio = 2.0f;
    float cutoff = 2000, reso = 0.3f, envAmt = 0.5f, keyTrack = 0.5f, filterDrive = 0.2f;
    float fA = 0.01f, fD = 0.4f, fS = 0.3f, fR = 0.5f;
    float aA = 0.005f, aD = 0.3f, aS = 0.8f, aR = 0.4f;
    float lfoRate = 4, lfoToPitch = 0, lfoToCutoff = 0;
    float glide = 0; bool mono = false; float tune = 0;   // tune in semitones
    // texture
    float wobble = 0.3f, lofi = 0.3f, haze = 0.4f, room = 0.2f, drive = 0;
    float delayTime = 0.375f, delayFb = 0.35f, delayMix = 0;
};

struct Oscillator
{
    float phase = 0, inc = 0, modPhase = 0;
    static inline float blep (float t, float dt)
    {
        if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
    inline float next (int wave, float pw, float fmIndex, float fmRatio)
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
            case 2: return 1.0f - 4.0f * std::abs (t - 0.5f);     // harmonics fall at 1/n^2: aliasing negligible
            case 3: return std::sin (t * 2.0f * kPi);
            default:
            {   // 2-operator FM (DX-style EP / bell): modulator at ratio, index scaled by the filter envelope upstream
                modPhase += inc * fmRatio; if (modPhase >= 1.0f) modPhase -= 1.0f;
                return std::sin (t * 2.0f * kPi + fmIndex * 6.0f * std::sin (modPhase * 2.0f * kPi));
            }
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

// 4-pole transistor ladder (Huovilainen-style tanh stages, 2x internal oversampling)
struct Ladder
{
    double sr = 44100; float s[4] {}; float g = 0, k = 0;
    void prepare (double rate) { sr = rate; reset(); }
    void reset() { for (auto& v : s) v = 0; }
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
        return out * (1.0f + k * 0.35f);
    }
};

class Voice
{
public:
    void prepare (double rate) { sr = rate; ladder.prepare (rate); fEnv.sr = rate; aEnv.sr = rate; }

    void noteOn (int midiNote, float velocity, const SynthParams& p, float glideFrom)
    {
        note = midiNote; vel = velocity; active = true; sustained = false; age = 0;
        targetFreq = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
        curFreq = (p.glide > 0.001f && glideFrom > 0) ? glideFrom : targetFreq;
        fEnv = {}; aEnv = {}; fEnv.sr = sr; aEnv.sr = sr;
        fEnv.a = p.fA; fEnv.d = p.fD; fEnv.s = p.fS; fEnv.r = p.fR;
        aEnv.a = p.aA; aEnv.d = p.aD; aEnv.s = p.aS; aEnv.r = p.aR;
        fEnv.noteOn(); aEnv.noteOn();
        juce::Random rnd; for (auto& o : osc) { o.phase = rnd.nextFloat(); o.modPhase = 0; }
    }
    void noteOff()  { fEnv.noteOff(); aEnv.noteOff(); sustained = false; }
    bool isActive() const { return active; }
    bool isReleasing() const { return aEnv.stage == ADSR::Release; }
    int  getNote() const { return note; }
    bool sustained = false;   // held by the sustain pedal
    juce::uint32 age = 0;

    inline float render (const SynthParams& p, float lfoVal, float globalPitch)
    {
        if (! active) return 0.0f;
        ++age;
        if (p.glide > 0.001f) curFreq += (targetFreq - curFreq) * (1.0f - std::exp (-1.0f / (p.glide * (float) sr)));
        else curFreq = targetFreq;
        float f = curFreq * globalPitch * std::pow (2.0f, lfoVal * p.lfoToPitch * 0.5f / 12.0f);
        float det2 = std::pow (2.0f, (p.osc2Detune / 1200.0f) + p.osc2Oct);
        float det3 = std::pow (2.0f, (p.osc3Detune / 1200.0f) + p.osc3Oct);
        float spread = p.fat * 0.012f;
        osc[0].inc = f / (float) sr; osc[1].inc = f * det2 / (float) sr; osc[2].inc = f * det3 / (float) sr;
        osc[3].inc = f * (1.0f + spread) / (float) sr; osc[4].inc = f * (1.0f - spread) / (float) sr;

        float fe = fEnv.next();
        float fmIdx = p.fm * (0.25f + 0.75f * fe);
        float x = osc[0].next (p.osc1Wave, p.pw, fmIdx, p.fmRatio) * p.osc1Lvl
                + osc[1].next (p.osc2Wave, p.pw, fmIdx, p.fmRatio) * p.osc2Lvl
                + osc[2].next (p.osc3Wave, p.pw, fmIdx, p.fmRatio) * p.osc3Lvl;
        if (p.fat > 0.001f)
            x += (osc[3].next (p.osc1Wave, p.pw, fmIdx, p.fmRatio) + osc[4].next (p.osc1Wave, p.pw, fmIdx, p.fmRatio)) * p.osc1Lvl * p.fat * 0.6f;
        x *= 0.35f;

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
    Oscillator osc[5];
    Ladder ladder; ADSR fEnv, aEnv;
};

// ---------------------------------------------------------------------------------------------
// Texture stage
// ---------------------------------------------------------------------------------------------
struct Texture
{
    double sr = 44100;
    // wow & flutter
    float wowPhase = 0, flutterPhase = 0, walk = 0; juce::Random rng;
    // lo-fi
    float holdL = 0, holdR = 0, holdAcc = 0;
    OnePole muffleL1, muffleL2, muffleR1, muffleR2, hissLP, tapePreL, tapePreR;
    DCBlock dcL, dcR;

    void prepare (double rate)
    {
        sr = rate; hissLP.setLowpass (5000.0f, rate); tapePreL.setLowpass (3000.0f, rate); tapePreR.setLowpass (3000.0f, rate);
        setMuffle (16000.0f);
    }
    void setMuffle (float hz) { for (auto* f : { &muffleL1, &muffleL2, &muffleR1, &muffleR2 }) f->setLowpass (hz, sr); }

    // Global pitch factor for this sample: slow wow (sine + random walk) and fast flutter, both scaled by `wobble`.
    inline float pitchFactor (float wobble)
    {
        wowPhase += 0.55f / (float) sr; if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += 6.8f / (float) sr; if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
        walk += (rng.nextFloat() - 0.5f) * 0.0025f; walk *= 0.99995f; walk = juce::jlimit (-1.0f, 1.0f, walk);
        float wow = 0.6f * std::sin (wowPhase * 2.0f * kPi) + 0.7f * walk;
        float flutter = std::sin (flutterPhase * 2.0f * kPi);
        float cents = wobble * (34.0f * wow + 5.0f * flutter * wobble);
        return std::pow (2.0f, cents / 1200.0f);
    }

    // 12-bit / sample-rate crunch -> muffle -> tape saturation -> hiss
    inline void lofi (float& l, float& r, float amt)
    {
        if (amt > 0.001f)
        {
            // sample-and-hold decimation: effective rate slides from full down to ~7 kHz
            float ratio = 1.0f + amt * amt * ((float) sr / 7000.0f - 1.0f);
            holdAcc += 1.0f;
            if (holdAcc >= ratio)
            {
                holdAcc -= ratio;
                float bits = 16.0f - amt * 8.0f;                     // 16 -> 8 bits
                float q = std::pow (2.0f, bits - 1.0f);
                float dither = (rng.nextFloat() - 0.5f) / q;
                holdL = std::round ((l + dither) * q) / q; holdR = std::round ((r + dither) * q) / q;
            }
            l = holdL; r = holdR;
            // tape: HF pre-emphasis into tanh so the top squashes first, then level restore
            float pl = l + 0.5f * (l - tapePreL.lp (l)), pr = r + 0.5f * (r - tapePreR.lp (r));
            float d = 1.0f + amt * 2.5f;
            l = fastTanh (pl * d) / std::pow (d, 0.7f); r = fastTanh (pr * d) / std::pow (d, 0.7f);
            // hiss
            float h = hissLP.lp (rng.nextFloat() * 2.0f - 1.0f) * amt * amt * 0.004f;
            l += h; r += h;
        }
        l = muffleL2.lp (muffleL1.lp (l)); r = muffleR2.lp (muffleR1.lp (r));
        l = dcL.process (l); r = dcR.process (r);
    }
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
        haze.prepare (spec); haze.setCentreDelay (9.0f); haze.setDepth (0.45f); haze.setRate (0.28f); haze.setFeedback (0.0f);
        delay.prepare (spec); delay.setMaximumDelayInSamples ((int) rate * 2);
        delayModLFO.setRate (0.31f, rate);
        delayLP[0].setLowpass (3500.0f, rate); delayLP[1].setLowpass (3500.0f, rate);
        texture.prepare (rate);
        room.prepare (rate, maxBlock);
        roomBuf.setSize (2, maxBlock);
        gateEnv.setLowpass (18.0f, rate);
        hp[0].setLowpass (25.0f, rate); hp[1].setLowpass (25.0f, rate);
        pitchSm.setTime (0.02f, rate); pitchSm.reset (1.0f);
    }

    void setParams (const SynthParams& p)
    {
        params = p; lfo.setRate (p.lfoRate, sr);
        haze.setMix (p.haze * 0.5f); haze.setDepth (0.25f + 0.5f * p.haze);
        texture.setMuffle (juce::jmap (p.lofi, 0.0f, 1.0f, 16000.0f, 2200.0f));
        aura::ReverbEngine::Params rp;
        rp.size = 0.22f; rp.decaySec = 0.35f + 0.5f * p.room; rp.damp = 0.7f; rp.lowcutHz = 200; rp.diffusion = 0.9f;
        rp.modRate = 0.4f; rp.modDepth = 0.3f; rp.width = 1.0f; rp.colour = aura::ReverbEngine::Vintage; rp.predelayMs = 8.0f;
        room.setParams (rp);
    }

    void handleMidi (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            float glideFrom = lastFreq;
            Voice* v = nullptr;
            if (params.mono) { v = &voices[0]; }
            else
            {
                for (auto& o : voices) if (! o.isActive()) { v = &o; break; }
                if (! v) { v = &voices[0]; for (auto& o : voices) if (o.age > v->age && (o.isReleasing() || ! v->isReleasing())) v = &o; }
            }
            v->noteOn (m.getNoteNumber(), m.getFloatVelocity(), params, glideFrom);
            lastFreq = (float) juce::MidiMessage::getMidiNoteInHertz (m.getNoteNumber());
        }
        else if (m.isNoteOff())
        {
            for (auto& v : voices)
                if (v.isActive() && v.getNote() == m.getNoteNumber()) { if (pedal) v.sustained = true; else v.noteOff(); }
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { for (auto& v : voices) v.noteOff(); }
        else if (m.isSustainPedalOn()) pedal = true;
        else if (m.isSustainPedalOff()) { pedal = false; for (auto& v : voices) if (v.sustained) v.noteOff(); }
        else if (m.isPitchWheel()) bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    }

    void render (float* L, float* R, int n)
    {
        const float staticPitch = std::pow (2.0f, (params.tune + bend * 2.0f) / 12.0f);
        for (int i = 0; i < n; ++i)
        {
            float lv = lfo.sine();
            float gp = pitchSm.process (staticPitch) * texture.pitchFactor (params.wobble);
            float mix = 0;
            for (auto& v : voices) mix += v.render (params, lv, gp);
            mix = fastTanh (mix * (1.0f + params.drive * 3.0f)) * (1.0f / (1.0f + params.drive));
            float l = mix, r = mix;
            texture.lofi (l, r, params.lofi);
            L[i] = hp[0].hp (l); R[i] = hp[1].hp (r);
        }
        float* chans[2] = { L, R };
        juce::dsp::AudioBlock<float> blk (chans, 2, (size_t) n);
        if (params.haze > 0.001f) haze.process (juce::dsp::ProcessContextReplacing<float> (blk));

        // dirty gated room: short vintage reverb, gated by the dry envelope (classic 80s "slap room")
        if (params.room > 0.001f)
        {
            roomBuf.setSize (2, n, false, false, true);
            room.process (L, R, roomBuf.getWritePointer (0), roomBuf.getWritePointer (1), n);
            const float* wl = roomBuf.getReadPointer (0); const float* wr = roomBuf.getReadPointer (1);
            for (int i = 0; i < n; ++i)
            {
                float env = gateEnv.lp (std::abs (L[i]) + std::abs (R[i]));
                float gate = juce::jlimit (0.0f, 1.0f, (env - 0.004f) * 60.0f);
                float g = params.room * 0.9f * gate;
                L[i] += wl[i] * g; R[i] += wr[i] * g;
            }
        }

        // modulated tape delay
        if (params.delayMix > 0.001f)
        {
            float base = juce::jlimit (1.0f, (float) sr * 1.9f, params.delayTime * (float) sr);
            for (int i = 0; i < n; ++i)
            {
                float mod = 1.0f + 0.004f * delayModLFO.sine() * (0.3f + params.wobble);
                float dl = delay.popSample (0, base * mod), dr = delay.popSample (1, base * mod * 1.003f);
                dl = delayLP[0].lp (dl); dr = delayLP[1].lp (dr);
                delay.pushSample (0, fastTanh (L[i] + dl * params.delayFb));
                delay.pushSample (1, fastTanh (R[i] + dr * params.delayFb));
                L[i] += dl * params.delayMix; R[i] += dr * params.delayMix;
            }
        }
        else for (int i = 0; i < n; ++i) { delay.pushSample (0, L[i]); delay.pushSample (1, R[i]); }
    }

    int activeVoices() const { int c = 0; for (auto& v : voices) c += v.isActive(); return c; }

private:
    double sr = 44100; SynthParams params; std::array<Voice, kVoices> voices; float lastFreq = 0; float bend = 0; bool pedal = false;
    LFO lfo, delayModLFO; OnePole hp[2], delayLP[2], gateEnv; Smooth pitchSm;
    juce::dsp::Chorus<float> haze;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 96000 * 2 };
    Texture texture; aura::ReverbEngine room; juce::AudioBuffer<float> roomBuf;
};
} // namespace mcgee
