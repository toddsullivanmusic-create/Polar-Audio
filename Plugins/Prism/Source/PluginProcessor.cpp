#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using APF = AudioParameterFloat;

static const StringArray kColorTypes  { "Warm", "Fuzz", "Crush", "Fold", "Sweep", "Squash" };
static const StringArray kMotionTypes { "Chorus", "Vibrato", "Phaser", "Tremolo", "Flanger", "Detune" };
static const StringArray kSpaceTypes  { "Echo", "Ping", "Reverse", "Shimmer", "Multi", "Grain" };
static const StringArray kAirTypes    { "Hall", "Plate", "Bloom", "Spring", "Cassette", "Glitch" };
static const char* const kModuleNames[4] = { "Color", "Motion", "Space", "Air" };

// ------------------------------------------------------------------------------------------------
const std::array<std::array<int, 4>, 24>& PrismProcessor::permutations()
{
    static const auto perms = []
    {
        std::array<std::array<int, 4>, 24> out {};
        std::array<int, 4> a { 0, 1, 2, 3 };
        int i = 0;
        do { out[(size_t) i++] = a; } while (std::next_permutation (a.begin(), a.end()));
        return out;
    }();
    return perms;
}

int PrismProcessor::permutationIndex (const std::array<int, 4>& order)
{
    auto& p = permutations();
    for (int i = 0; i < 24; ++i) if (p[(size_t) i] == order) return i;
    return 0;
}

std::array<int, 4> PrismProcessor::currentOrder() const { return permutations()[(size_t) jlimit (0, 23, (int) p (PID::order))]; }

// ------------------------------------------------------------------------------------------------
AudioProcessorValueTreeState::ParameterLayout PrismProcessor::createLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = [] (const String& id, const String& name, float def) {
        return std::make_unique<APF> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f, 0.001f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; })
                                           .withValueFromStringFunction ([] (const String& s) { return s.getFloatValue() / 100.0f; })); };

    const StringArray* types[4] = { &kColorTypes, &kMotionTypes, &kSpaceTypes, &kAirTypes };
    const float defAmt[4] = { 0.25f, 0.4f, 0.35f, 0.45f }, defTw[4] = { 0.5f, 0.35f, 0.45f, 0.5f };
    const float defMix[4] = { 1.0f, 1.0f, 0.22f, 0.28f };
    const bool defOn[4] = { true, false, true, true };
    for (int m = 0; m < 4; ++m)
    {
        const String nm = kModuleNames[m];
        auto group = std::make_unique<AudioProcessorParameterGroup> (PID::modules[m], nm, " | ");
        group->addChild (std::make_unique<AudioParameterChoice> (ParameterID { PID::m (m, PID::type), 1 }, nm + " Type", *types[m], 0));
        group->addChild (pct (PID::m (m, PID::amount), nm + " Amount", defAmt[m]));
        group->addChild (pct (PID::m (m, PID::tweak), nm + " Tweak", defTw[m]));
        group->addChild (pct (PID::m (m, PID::tone), nm + " Tone", 0.5f));
        group->addChild (pct (PID::m (m, PID::mix), nm + " Mix", defMix[m]));
        group->addChild (std::make_unique<AudioParameterBool> (ParameterID { PID::m (m, PID::on), 1 }, nm + " On", defOn[m]));
        l.add (std::move (group));
    }

    StringArray orders;
    for (auto& o : permutations())
        orders.add (String (kModuleNames[o[0]]) + " > " + kModuleNames[o[1]] + " > " + kModuleNames[o[2]] + " > " + kModuleNames[o[3]]);

    auto dbText = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; });
    l.add (std::make_unique<APF> (ParameterID { PID::input, 1 }, "Input", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, dbText));
    l.add (std::make_unique<APF> (ParameterID { PID::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dbText));
    l.add (pct (PID::gmix, "Mix", 1.0f));
    l.add (std::make_unique<APF> (ParameterID { PID::tilt, 1 }, "Tilt", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v < -0.005f ? "Dark " + String (roundToInt (-v * 6.0f * 10.0f) / 10.0f, 1) + " dB" : v > 0.005f ? "Bright " + String (roundToInt (v * 60.0f) / 10.0f, 1) + " dB" : String ("Flat"); })));
    l.add (std::make_unique<APF> (ParameterID { PID::width, 1 }, "Width", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f,
        AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return v < 0.5f ? String ("Mono") : String (roundToInt (v)) + " %"; })));
    l.add (pct (PID::duck, "Duck", 0.0f));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::order, 1 }, "Chain Order", orders, 0));
    l.add (pct (PID::padX, "Pad X", 0.5f));
    l.add (pct (PID::padY, "Pad Y", 0.5f));
    l.add (pct (PID::padDepth, "Pad Depth", 0.6f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::freeze, 1 }, "Freeze", false));
    l.add (pct (PID::morph, "Scene Morph", 0.0f));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::sync, 1 }, "Tempo Sync", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { PID::gestureLen, 1 }, "Gesture Length", StringArray { "1 bar", "2 bars", "4 bars", "8 bars" }, 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { PID::safety, 1 }, "Safety Limiter", true));
    return l;
}

// ------------------------------------------------------------------------------------------------
std::vector<polar::Preset> PrismProcessor::factoryPresets()
{
    struct M { int type; float amount, tweak, tone, mix; bool on; };
    auto make = [] (const char* name, M c, M mo, M s, M a, std::map<String, float> extra = {})
    {
        std::map<String, float> v {
            { PID::input, 0 }, { PID::output, 0 }, { PID::gmix, 1 }, { PID::tilt, 0 }, { PID::width, 100 }, { PID::duck, 0 },
            { PID::order, 0 }, { PID::padX, 0.5f }, { PID::padY, 0.5f }, { PID::padDepth, 0.6f }, { PID::freeze, 0 }, { PID::sync, 1 } };
        const M ms[4] = { c, mo, s, a };
        for (int m = 0; m < 4; ++m)
        {
            v[PID::m (m, PID::type)] = (float) ms[m].type; v[PID::m (m, PID::amount)] = ms[m].amount; v[PID::m (m, PID::tweak)] = ms[m].tweak;
            v[PID::m (m, PID::tone)] = ms[m].tone; v[PID::m (m, PID::mix)] = ms[m].mix; v[PID::m (m, PID::on)] = ms[m].on ? 1.0f : 0.0f;
        }
        for (auto& kv : extra) v[kv.first] = kv.second;
        return polar::Preset { name, v };
    };
    using C = prism::ColorModule; using Mo = prism::MotionModule; using S = prism::SpaceModule; using A = prism::AirModule;
    return {
        make ("Clean Canvas",       { C::Warm, .20f, .50f, .50f, 1, true },  { Mo::Chorus, .40f, .35f, .60f, 1, false }, { S::Echo, .35f, .58f, .40f, .20f, true },    { A::Hall, .45f, .50f, .50f, .25f, true }),
        make ("Dream Machine",      { C::Warm, .25f, .55f, .55f, 1, true },  { Mo::Chorus, .50f, .30f, .70f, 1, true },  { S::Shimmer, .55f, .62f, .45f, .30f, true }, { A::Bloom, .60f, .40f, .60f, .45f, true }),
        make ("Tape Memories",      { C::Squash, .35f, .30f, .40f, 1, true },{ Mo::Vibrato, .25f, .45f, .70f, 1, true }, { S::Echo, .45f, .58f, .30f, .30f, true },    { A::Cassette, .50f, .55f, .45f, 1, true }),
        make ("Fuzz Cathedral",     { C::Fuzz, .60f, .20f, .45f, 1, true },  { Mo::Phaser, .50f, .25f, .50f, 1, true },  { S::Reverse, .40f, .62f, .50f, .35f, true }, { A::Hall, .80f, .90f, .40f, .50f, true }),
        make ("8-Bit Arcade",       { C::Crush, .70f, .60f, .60f, 1, true }, { Mo::Tremolo, .60f, .70f, .90f, 1, true }, { S::Ping, .40f, .42f, .55f, .30f, true },    { A::Glitch, .50f, .30f, .40f, 1, true }),
        make ("Wavefold Garden",    { C::Fold, .50f, .60f, .50f, 1, true },  { Mo::Flanger, .50f, .20f, .70f, 1, true }, { S::Multi, .45f, .70f, .50f, .35f, true },   { A::Spring, .50f, .50f, .60f, .30f, true }),
        make ("Funk Envelope",      { C::Sweep, .70f, .30f, .60f, 1, true }, { Mo::Chorus, .30f, .30f, .60f, 1, false }, { S::Ping, .30f, .42f, .50f, .20f, true },    { A::Spring, .40f, .40f, .50f, .25f, true }),
        make ("Granular Haze",      { C::Warm, .15f, .50f, .50f, 1, true },  { Mo::Detune, .45f, .40f, .60f, 1, true },  { S::Grain, .55f, .60f, .30f, .45f, true },   { A::Bloom, .70f, .50f, .30f, .50f, true }),
        make ("Vocal Doubler",      { C::Squash, .45f, .25f, .50f, 1, true },{ Mo::Detune, .35f, .20f, .70f, 1, true },  { S::Echo, .25f, .25f, .55f, .15f, true },    { A::Plate, .45f, .40f, .55f, .20f, true }, { { PID::duck, .4f } }),
        make ("Ambient Swell",      { C::Warm, .20f, .50f, .50f, 1, false }, { Mo::Chorus, .35f, .30f, .60f, 1, true },  { S::Shimmer, .70f, .70f, .50f, .40f, true }, { A::Hall, .95f, 1.0f, .45f, .60f, true }),
        make ("Broken Radio",       { C::Crush, .40f, .80f, .30f, 1, true }, { Mo::Vibrato, .45f, .55f, 1.0f, 1, true }, { S::Reverse, .40f, .50f, .50f, .30f, false },{ A::Cassette, .80f, .90f, .25f, 1, true }),
        make ("Stutter Drums",      { C::Squash, .60f, .70f, .20f, 1, true },{ Mo::Chorus, .30f, .30f, .60f, 1, false }, { S::Multi, .35f, .58f, .60f, .25f, true },   { A::Glitch, .70f, .10f, .50f, 1, true }),
        make ("Wobble Keys",        { C::Warm, .35f, .55f, .60f, 1, true },  { Mo::Vibrato, .30f, .70f, .10f, 1, true }, { S::Echo, .30f, .50f, .50f, .20f, false },   { A::Spring, .35f, .30f, .55f, .20f, true }),
        make ("Seasick Shoegaze",   { C::Fuzz, .45f, .05f, .35f, 1, true },  { Mo::Flanger, .65f, .15f, .80f, 1, true }, { S::Reverse, .55f, .70f, .40f, .45f, true }, { A::Bloom, .85f, .30f, .80f, .55f, true }, { { PID::order, 1 } }),
        make ("Ping Pong Plate",    { C::Warm, .10f, .50f, .50f, 1, true },  { Mo::Chorus, .30f, .25f, .60f, 1, true },  { S::Ping, .45f, .62f, .60f, .30f, true },    { A::Plate, .55f, .60f, .60f, .30f, true }),
        make ("Lo-Fi Bedroom",      { C::Warm, .40f, .60f, .30f, 1, true },  { Mo::Detune, .30f, .50f, .40f, 1, true },  { S::Echo, .35f, .50f, .20f, .20f, true },    { A::Cassette, .55f, .60f, .40f, 1, true }),
    };
}

// ------------------------------------------------------------------------------------------------
PrismProcessor::PrismProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true).withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undo, "PRISM", createLayout()),
      presets (apvts, "PRISM", factoryPresets())
{
    startTimerHz (60);
}

bool PrismProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet(), in = layouts.getMainInputChannelSet();
    if (out != AudioChannelSet::stereo()) return false;
    return in == AudioChannelSet::stereo() || in == AudioChannelSet::mono();
}

void PrismProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (auto* m : mods) m->prepare (sampleRate, samplesPerBlock);
    latencySamples = color.latency();
    setLatencySamples (roundToInt (latencySamples));
    for (auto& d : dryDelay) d.prepare ((int) (sampleRate * 0.01) + 64);
    dryBuf.setSize (2, samplesPerBlock); duckBuf.setSize (1, samplesPerBlock);
    duckEnv.set (4.0f, 260.0f, sampleRate);
    for (int c = 0; c < 2; ++c) { lowShelf[c].reset(); highShelf[c].reset(); }
    lastTilt = -99.0f;
    inG.reset (sampleRate, 0.03); outG.reset (sampleRate, 0.03); mixS.reset (sampleRate, 0.03); widthS.reset (sampleRate, 0.05);
    inG.setCurrentAndTargetValue (Decibels::decibelsToGain (p (PID::input)));
    outG.setCurrentAndTargetValue (Decibels::decibelsToGain (p (PID::output)));
    mixS.setCurrentAndTargetValue (p (PID::gmix)); widthS.setCurrentAndTargetValue (p (PID::width) * 0.01f);
}

void PrismProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);

    // host transport
    double bpm = 120.0, ppq = 0.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    hostBpm.store (bpm); hostPpq.store (ppq); hostPlaying.store (playing);

    // input gain
    inG.setTargetValue (Decibels::decibelsToGain (p (PID::input)));
    float inPk = 0;
    for (int i = 0; i < n; ++i) { const float g = inG.getNextValue(); L[i] *= g; R[i] *= g; inPk = jmax (inPk, std::abs (L[i]), std::abs (R[i])); }
    inLevel.store (jmax (inPk, inLevel.load() * 0.9f));

    // dry (latency-aligned with the oversampled chain) and ducking envelope
    dryBuf.setSize (2, n, false, false, true); duckBuf.setSize (1, n, false, false, true);
    float* dl = dryBuf.getWritePointer (0); float* dr = dryBuf.getWritePointer (1); float* dk = duckBuf.getWritePointer (0);
    const float duckAmt = p (PID::duck);
    for (int i = 0; i < n; ++i)
    {
        dryDelay[0].push (L[i]); dryDelay[1].push (R[i]);
        dl[i] = latencySamples > 2.01f ? dryDelay[0].read (latencySamples) : L[i];
        dr[i] = latencySamples > 2.01f ? dryDelay[1].read (latencySamples) : R[i];
        const float e = duckEnv.process ((L[i] + R[i]) * 0.5f);
        dk[i] = 1.0f - duckAmt * jlimit (0.0f, 1.0f, e * 5.0f);
    }

    // XY pad offsets (live pad, or the recorded gesture when it is playing)
    const bool gesturePlaying = gState.load() == GestureState::Playing;
    const float px = gesturePlaying ? effPadX.load() : p (PID::padX);
    const float py = gesturePlaying ? effPadY.load() : p (PID::padY);
    if (! gesturePlaying) { effPadX.store (px); effPadY.store (py); }
    const float depth = p (PID::padDepth);
    const float offX = (px - 0.5f) * depth * 1.2f, offY = (py - 0.5f) * depth * 1.2f;

    const bool frz = p (PID::freeze) > 0.5f, sync = p (PID::sync) > 0.5f;
    for (int idx : currentOrder())
    {
        prism::Ctx c;
        c.type = (int) p (PID::m (idx, PID::type));
        c.amount = prism::clamp01 (p (PID::m (idx, PID::amount)) + offY);
        c.tweak = prism::clamp01 (p (PID::m (idx, PID::tweak)) + offX);
        c.tone = p (PID::m (idx, PID::tone));
        c.mix = p (PID::m (idx, PID::mix));
        c.on = p (PID::m (idx, PID::on)) > 0.5f;
        c.freeze = frz; c.sync = sync; c.bpm = bpm; c.ppq = ppq; c.playing = playing;
        c.duck = (idx >= 2) ? dk : nullptr;
        mods[(size_t) idx]->process (L, R, n, c);
    }

    // global tilt EQ (complementary shelves around 650 Hz)
    const float tilt = p (PID::tilt);
    if (std::abs (tilt - lastTilt) > 1.0e-4f)
    {
        lastTilt = tilt;
        auto lo = dsp::IIR::Coefficients<float>::makeLowShelf (sr, 650.0, 0.7f, Decibels::decibelsToGain (-tilt * 6.0f));
        auto hi = dsp::IIR::Coefficients<float>::makeHighShelf (sr, 650.0, 0.7f, Decibels::decibelsToGain (tilt * 6.0f));
        for (int c = 0; c < 2; ++c) { lowShelf[c].coefficients = lo; highShelf[c].coefficients = hi; }
    }
    const bool doTilt = std::abs (tilt) > 1.0e-4f;

    widthS.setTargetValue (p (PID::width) * 0.01f);
    mixS.setTargetValue (p (PID::gmix));
    outG.setTargetValue (Decibels::decibelsToGain (p (PID::output)));
    const bool safety = p (PID::safety) > 0.5f;
    float pkL = 0, pkR = 0;
    int sw = scopeWrite.load();
    for (int i = 0; i < n; ++i)
    {
        float l = L[i], r = R[i];
        if (doTilt) { l = highShelf[0].processSample (lowShelf[0].processSample (l)); r = highShelf[1].processSample (lowShelf[1].processSample (r)); }
        const float w = widthS.getNextValue();
        const float mid = (l + r) * 0.5f, side = (l - r) * 0.5f * w;
        l = mid + side; r = mid - side;
        const float m = mixS.getNextValue();
        l = dl[i] + (l - dl[i]) * m; r = dr[i] + (r - dr[i]) * m;
        const float g = outG.getNextValue();
        l *= g; r *= g;
        if (safety)
        {
            auto soft = [] (float x) { const float a = std::abs (x); return a < 0.9f ? x : std::copysign (0.9f + 0.1f * prism::fastTanh ((a - 0.9f) * 10.0f), x); };
            l = soft (l); r = soft (r);
        }
        L[i] = l; R[i] = r;
        pkL = jmax (pkL, std::abs (l)); pkR = jmax (pkR, std::abs (r));
        if ((i & 1) == 0) { scope[(size_t) sw] = (l + r) * 0.5f; sw = (sw + 1) % kScope; }
    }
    scopeWrite.store (sw);
    float cur = meterL.load(); if (pkL > cur) meterL.store (pkL);
    cur = meterR.load(); if (pkR > cur) meterR.store (pkR);
}

// ------------------------------------------------------------------------------------------------
// Scenes, gesture, dice (message thread)
// ------------------------------------------------------------------------------------------------
static bool excludedFromMorph (const String& id)
{
    return id == PID::morph || id == PID::padX || id == PID::padY || id == PID::gestureLen || id == PID::freeze || id == PID::safety;
}

void PrismProcessor::captureScene (int slot)
{
    auto& s = scene[(size_t) slot];
    s.clear();
    for (auto* prm : getParameters()) s.push_back (prm->getValue());
    sceneValid[(size_t) slot] = true;
    lastMorph = -1.0f;
}

void PrismProcessor::armGesture() { gState.store (GestureState::Armed); }
void PrismProcessor::playGesture (bool play) { gState.store (play ? GestureState::Playing : GestureState::Idle); }
void PrismProcessor::clearGesture()
{
    gState.store (GestureState::Idle);
    gestX.fill (0.5f); gestY.fill (0.5f);
}

void PrismProcessor::timerCallback()
{
    // --- scene morph: interpolate continuous params, switch discrete ones at the midpoint
    const float morph = p (PID::morph);
    if (sceneValid[0] && sceneValid[1] && std::abs (morph - lastMorph) > 1.0e-4f)
    {
        lastMorph = morph;
        auto& ps = getParameters();
        for (int i = 0; i < ps.size() && i < (int) scene[0].size(); ++i)
        {
            auto* prm = dynamic_cast<RangedAudioParameter*> (ps[i]);
            if (prm == nullptr || excludedFromMorph (prm->getParameterID())) continue;
            const float a = scene[0][(size_t) i], b = scene[1][(size_t) i];
            const float v = prm->isDiscrete() ? (morph < 0.5f ? a : b) : a + (b - a) * morph;
            if (std::abs (prm->getValue() - v) > 1.0e-5f) prm->setValueNotifyingHost (v);
        }
    }

    // --- gesture clock
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    const double dt = lastTick > 0.0 ? jlimit (0.0, 0.1, now - lastTick) : 0.0;
    lastTick = now;
    static constexpr double bars[] = { 1, 2, 4, 8 };
    const double lenBeats = bars[jlimit (0, 3, (int) p (PID::gestureLen))] * 4.0;
    double phase;
    if (hostPlaying.load()) phase = std::fmod (jmax (0.0, hostPpq.load()), lenBeats) / lenBeats;
    else { freePhase += dt * hostBpm.load() / 60.0 / lenBeats; freePhase -= std::floor (freePhase); phase = freePhase; }
    gPhase.store ((float) phase);
    const int idx = jlimit (0, kGesturePoints - 1, (int) (phase * kGesturePoints));

    auto st = gState.load();
    if (st == GestureState::Armed)
    {
        gestX.fill (p (PID::padX)); gestY.fill (p (PID::padY));
        lastRecIdx = idx; recStartPhase = (float) phase; recWrapped = false;
        gState.store (GestureState::Recording);
    }
    else if (st == GestureState::Recording)
    {
        const float x = p (PID::padX), y = p (PID::padY);
        int i = lastRecIdx;
        int steps = 0;
        while (i != idx && steps < kGesturePoints)
        {
            i = (i + 1) % kGesturePoints; ++steps;
            gestX[(size_t) i] = x; gestY[(size_t) i] = y;
            if (i == (int) (recStartPhase * kGesturePoints)) recWrapped = true;
        }
        lastRecIdx = idx;
        effPadX.store (x); effPadY.store (y);
        if (recWrapped) gState.store (GestureState::Playing);
    }
    else if (st == GestureState::Playing)
    {
        const float f = (float) (phase * kGesturePoints);
        const int i0 = (int) f % kGesturePoints, i1 = (i0 + 1) % kGesturePoints;
        const float t = f - std::floor (f);
        effPadX.store (gestX[(size_t) i0] + (gestX[(size_t) i1] - gestX[(size_t) i0]) * t);
        effPadY.store (gestY[(size_t) i0] + (gestY[(size_t) i1] - gestY[(size_t) i0]) * t);
    }
}

void PrismProcessor::randomise (const std::array<bool, 4>& locks)
{
    Random rnd;
    auto set = [this] (const String& id, float v) { if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
    for (int m = 0; m < 4; ++m)
    {
        if (locks[(size_t) m]) continue;
        const int t = rnd.nextInt (6);
        set (PID::m (m, PID::type), (float) t);
        set (PID::m (m, PID::amount), 0.2f + rnd.nextFloat() * 0.55f);
        set (PID::m (m, PID::tweak), 0.15f + rnd.nextFloat() * 0.7f);
        set (PID::m (m, PID::tone), 0.2f + rnd.nextFloat() * 0.6f);
        float mix = 1.0f;
        if (m == 2) mix = 0.18f + rnd.nextFloat() * 0.25f;
        if (m == 3) mix = prism::AirModule::isCompleteEffect (t) ? 1.0f : 0.22f + rnd.nextFloat() * 0.3f;
        set (PID::m (m, PID::mix), mix);
        set (PID::m (m, PID::on), rnd.nextFloat() < 0.85f ? 1.0f : 0.0f);
    }
}

// ------------------------------------------------------------------------------------------------
static String floatsToString (const float* v, size_t n)
{
    StringArray a; for (size_t i = 0; i < n; ++i) a.add (String (v[i], 4)); return a.joinIntoString (",");
}
static std::vector<float> stringToFloats (const String& s)
{
    std::vector<float> out; for (auto& t : StringArray::fromTokens (s, ",", "")) out.push_back (t.getFloatValue()); return out;
}

void PrismProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    for (int sIdx = 0; sIdx < 2; ++sIdx)
        if (sceneValid[(size_t) sIdx]) state.setProperty ("scene" + String (sIdx), floatsToString (scene[(size_t) sIdx].data(), scene[(size_t) sIdx].size()), nullptr);
    state.setProperty ("gestX", floatsToString (gestX.data(), gestX.size()), nullptr);
    state.setProperty ("gestY", floatsToString (gestY.data(), gestY.size()), nullptr);
    state.setProperty ("gestPlaying", gState.load() == GestureState::Playing, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void PrismProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml) return;
    auto state = ValueTree::fromXml (*xml);
    for (int sIdx = 0; sIdx < 2; ++sIdx)
    {
        auto v = stringToFloats (state.getProperty ("scene" + String (sIdx)).toString());
        sceneValid[(size_t) sIdx] = v.size() == (size_t) getParameters().size();
        if (sceneValid[(size_t) sIdx]) scene[(size_t) sIdx] = v;
    }
    auto gx = stringToFloats (state.getProperty ("gestX").toString()), gy = stringToFloats (state.getProperty ("gestY").toString());
    if (gx.size() == (size_t) kGesturePoints && gy.size() == (size_t) kGesturePoints)
    {
        std::copy (gx.begin(), gx.end(), gestX.begin()); std::copy (gy.begin(), gy.end(), gestY.begin());
        gState.store ((bool) state.getProperty ("gestPlaying") ? GestureState::Playing : GestureState::Idle);
    }
    apvts.replaceState (state);
    lastMorph = p (PID::morph);
}

AudioProcessorEditor* PrismProcessor::createEditor() { return new PrismEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PrismProcessor(); }
