# Polar Audio — four Logic Pro plugins

| | Plugin | Type | What it is |
|---|---|---|---|
| 🔵 | **AURA** | Audio Unit effect | Algorithmic reverb: 8-line modulated FDN, 4 colour circuits (Modern / Vintage / Dark / Plate), shimmer, freeze, ducking, width |
| 🟠 | **GLOW** | Audio Unit effect | 4× oversampled saturation: Tube / Tape / Transformer / Crunch circuits, 3-band Focus, Dynamics, tilt Tone, auto-gain |
| 🟢 | **McGEE** | Audio Unit instrument | Lo-fi analog polysynth built for the "underwater" Mk.gee-style texture: 3 alias-free oscillators (+ FM Keys), transistor-ladder filter with drive, 16 voices, unison Fat, then a Texture stage — cassette **Wobble** (wow & flutter), **Lo-fi** (12-bit / sample-rate crunch, muffle, tape saturation, hiss), **Haze** chorus, dirty gated **Room**, modulated tape delay. Sustain pedal + pitch bend. |
| 🌈 | **PRISM** | Audio Unit effect | Four-module multi-effect pedal: **Color** (Warm, Fuzz, Crush, Fold, Sweep, Squash) → **Motion** (Chorus, Vibrato, Phaser, Tremolo, Flanger, Detune) → **Space** (Echo, Ping, Reverse, Shimmer, Multi, Grain) → **Air** (Hall, Plate, Bloom, Spring, Cassette, Glitch). Reorderable chain, XY pad with tempo-synced gesture recorder, scene A/B morph, Dice with module locks, Freeze, ducking, trails, tempo sync. 2× linear-phase oversampled drive, Hermite-interpolated delays, click-free type crossfades. White pedal-style UI. |
| 🟣 | **SANCTUARY** | Audio Unit instrument | Live worship pad engine: generated pads in any key, major/minor, 5 styles, seamless key crossfade, MIDI key switching, mod-wheel swell, one-tap fade, bar-synced motion, AURA reverb built in |

All four share one design system (`Shared/UI`): dark-glass panels, glowing rotary arcs, iOS-style switches, a reactive particle/aurora backdrop, factory + user presets, A/B, resizable windows with locked aspect ratio.

Market reasoning: [`Marketing/MARKET_ANALYSIS.md`](Marketing/MARKET_ANALYSIS.md). Launch plan: [`Marketing/MARKETING.md`](Marketing/MARKETING.md).

## Install in Logic Pro (no build needed)

Every push to `main` builds the Audio Units on a macOS runner, runs Apple's `auval` on them, and publishes the result as the
**"latest"** release: <https://github.com/toddsullivanmusic-create/polar-audio/releases/tag/latest>

1. Download `PolarAudio-macOS.zip` from that page and unzip it.
2. Run the installer. On macOS Sequoia (15+) Gatekeeper blocks downloaded scripts, so open Terminal and paste:
   ```bash
   cd "$(ls -d ~/Downloads/PolarAudio-macOS*/ | head -1)" && xattr -cr . && bash "Install Polar Plugins.command"
   ```
   (Alternative: *System Settings → Privacy & Security → Security → Open Anyway*.) On older macOS, just double-click **Install Polar Plugins.command**.
3. Open Logic Pro. Effects: *Audio Units → Polar Audio → AURA / GLOW / PRISM*. Instruments: *AU Instruments → Polar Audio → McGEE / SANCTUARY*.

Details and troubleshooting in `Installer/INSTALL.txt`. Versioned releases are published for `v*` tags.

## Build for Logic Pro yourself (macOS)

Logic loads **Audio Units**. AU can only be compiled on macOS with Xcode; this repo is set up so that is a three-command job.

```bash
# prerequisites: Xcode 15+, CMake 3.22+ (brew install cmake)
# JUCE is fetched automatically by CMake (or put a checkout at ../JUCE to reuse one)
cmake -B build -G Xcode -DPOLAR_BUILD_TESTS=OFF
cmake --build build --config Release
```

`COPY_PLUGIN_AFTER_BUILD` is on, so each AU is installed to `~/Library/Audio/Plug-Ins/Components/` automatically. Then:

```bash
auval -a | grep Polr          # all four should validate
killall -9 AudioComponentRegistrar  # if Logic doesn't see them, reset the AU cache and relaunch Logic
```

In Logic: effects live under **Audio Units → Polar Audio**, instruments under **AU Instruments → Polar Audio**. Universal binaries (arm64 + x86_64), macOS 11+.

For distribution to customers: codesign with a Developer ID, notarize, and ship a `.pkg`. (`HARDENED_RUNTIME_ENABLED` is already set in CMake.)

## Build on Linux / Windows (VST3 + Standalone)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Verify the engines (headless)

```bash
cmake --build build --target TestAura TestGlow TestMcGee TestSanctuary
for t in TestAura TestGlow TestMcGee TestSanctuary; do ./build/Tests/${t}_artefacts/Release/$t build/renders; done
```

Each test instantiates the processor, walks every factory preset, renders 6 s through the real `processBlock`, checks for NaN/inf/silence/overs, and writes a `.wav` of preset 0 to `build/renders/`.

## Layout

```
Shared/UI        Theme, LookAndFeel, Knob/Choice/Toggle/Panel/Header/Meter/Backdrop, PresetManager, A/B
Shared/DSP       delay lines, allpass, one-pole, smoothers, fast tanh, DC blocker
Plugins/Aura     ReverbEngine.h (FDN core), processor, editor
Plugins/Glow     SaturationEngine.h, processor, editor
Plugins/McGee    SynthEngine.h (oscillators, ladder, voices, FX), processor, editor
Plugins/Sanctuary PadEngine.h (pad voices, crossfade, motion, AURA reverb), processor, editor
Tests            headless engine verification
Marketing        market analysis + go-to-market
Docs/screenshots GUI captures
```

## SANCTUARY — how it's played live

- Tap a key on the wheel, or play **C1–B1** (major) / **C2–B2** (minor) on any MIDI keyboard; the pad crossfades into the new key over *Transition* seconds without a gap.
- **C3** toggles play/stop, **D3** fades out over *Fade* seconds. **Mod wheel** = Swell. **Program change 0–23** = key/mode (for Planning Center / set-list controllers).
- *Motion* moves the filter in time with Logic's tempo over 1–16 bars. *Air* adds the shimmer layer, *Depth* the sub, *Colour* the 3rd.
- Put it on a software-instrument track, arm it, press Play once — it runs for the whole service.

## PRISM — how it plays

- **Four modules, 24 effects.** Each module has a type selector, a big **Amount** knob, **Tweak**, **Tone** and **Mix** (labels change with the effect), and a footswitch. Bypassed delays and reverbs keep ringing out (trails).
- **Reorder the chain** with the ‹ › arrows on each module (all 24 orders; automatable as *Chain Order*).
- **XY pad**: X pushes every module's Tweak, Y pushes every Amount (scaled by *Pad Depth*). Hit **REC** and move the puck: one loop (1–8 bars, locked to Logic's transport) is captured and plays back. Double-click the pad to recentre.
- **Scenes**: dial a sound → **Set A**, dial another → **Set B**, then sweep the morph slider (automatable) between them.
- **Dice** randomises every module that isn't **LOCK**ed; **Undo** takes it back.
- **Freeze** holds the Space and Air tails indefinitely. **Duck** pushes delays and reverbs down while you play. **Tempo sync** snaps delay times to note values.
- **Glitch** (Air) is a beat-repeater locked to the host grid: Chance, Slice length, and a pitch-falling Tape-stop amount.
