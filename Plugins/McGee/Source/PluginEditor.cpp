#include "PluginEditor.h"

McGeeEditor::McGeeEditor (McGeeProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (backdrop); addAndMakeVisible (header);
    for (auto* c : { &oscPanel, &filterPanel, &envPanel, &modPanel, &texturePanel, &fxPanel }) addAndMakeVisible (c);
    addAndMakeVisible (scope);
    for (auto* c : { &w1, &w2, &w3, &o2, &o3 }) addAndMakeVisible (c);
    for (auto* k : { &l1, &l2, &l3, &d2, &d3, &pw, &fat, &fm, &fmRatio, &cutoff, &reso, &envAmt, &keyTrack, &fDrive, &fA, &fD, &fS, &fR, &aA, &aD, &aS, &aR,
                     &lfoRate, &lfoPitch, &lfoCutoff, &glide, &tune, &wobble, &lofi, &haze, &room, &drive, &delayTime, &delayFb, &delayMix, &volume })
        addAndMakeVisible (k);
    addAndMakeVisible (mono); addAndMakeVisible (meter);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, theme.bg2.brighter (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, theme.bg0);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, theme.accent.withAlpha (0.7f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, theme.accent.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, theme.bg0.withAlpha (0.5f));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, juce::Colours::transparentBlack);
    keyboard.setAvailableRange (36, 96);
    addAndMakeVisible (keyboard);
    proc.presets.populate (header.presetBox);
    header.abButton.onClick = [this] { ab.toggle (proc.apvts); };
    setResizable (true, true);
    setResizeLimits (1000, 640, 1700, 1088);
    getConstrainer()->setFixedAspectRatio (1120.0 / 716.0);
    setSize (1120, 716);
}

McGeeEditor::~McGeeEditor() { setLookAndFeel (nullptr); }

void McGeeEditor::resized()
{
    auto r = getLocalBounds();
    backdrop.setBounds (r);
    header.setBounds (r.removeFromTop (56));
    keyboard.setBounds (r.removeFromBottom (60).reduced (14, 4));
    r.reduce (14, 10);
    const int gap = 10;

    // Row 1: oscillators + scope
    auto row1 = r.removeFromTop ((int) (r.getHeight() * 0.36f));
    scope.setBounds (row1.removeFromRight ((int) (row1.getWidth() * 0.2f)));
    row1.removeFromRight (gap);
    oscPanel.setBounds (row1);
    auto oc = oscPanel.content().translated (oscPanel.getX(), oscPanel.getY());
    int colW = oc.getWidth() / 3;
    auto col = [&] (polar::Choice& wave, polar::Choice* oct, std::initializer_list<polar::Knob*> knobs)
    {
        auto c = oc.removeFromLeft (colW).reduced (4, 0);
        auto top = c.removeFromTop (46);
        if (oct) { oct->setBounds (top.removeFromRight (64)); top.removeFromRight (6); }
        wave.setBounds (top);
        c.removeFromTop (2);
        int kw = c.getWidth() / (int) knobs.size();
        for (auto* k : knobs) k->setBounds (c.removeFromLeft (kw));
    };
    col (w1, nullptr, { &l1, &pw, &fat, &fm, &fmRatio });
    col (w2, &o2, { &l2, &d2 });
    col (w3, &o3, { &l3, &d3 });

    r.removeFromTop (gap);
    // Row 2: filter | envelopes
    auto row2 = r.removeFromTop ((int) (r.getHeight() * 0.48f));
    filterPanel.setBounds (row2.removeFromLeft ((int) (row2.getWidth() * 0.42f)));
    row2.removeFromLeft (gap);
    envPanel.setBounds (row2);
    auto fc = filterPanel.content().translated (filterPanel.getX(), filterPanel.getY());
    cutoff.setBounds (fc.removeFromLeft ((int) (fc.getWidth() * 0.3f)));
    int fw = fc.getWidth() / 4;
    for (auto* k : { &reso, &envAmt, &keyTrack, &fDrive }) k->setBounds (fc.removeFromLeft (fw).reduced (2));
    auto ec = envPanel.content().translated (envPanel.getX(), envPanel.getY());
    auto filt = ec.removeFromLeft (ec.getWidth() / 2);
    int ew = filt.getWidth() / 4;
    for (auto* k : { &fA, &fD, &fS, &fR }) k->setBounds (filt.removeFromLeft (ew).reduced (2));
    for (auto* k : { &aA, &aD, &aS, &aR }) k->setBounds (ec.removeFromLeft (ew).reduced (2));

    r.removeFromTop (gap);
    // Row 3: modulation | texture | tape delay & output
    int w3 = (r.getWidth() - 2 * gap);
    modPanel.setBounds (r.removeFromLeft ((int) (w3 * 0.34f))); r.removeFromLeft (gap);
    texturePanel.setBounds (r.removeFromLeft ((int) (w3 * 0.30f))); r.removeFromLeft (gap);
    fxPanel.setBounds (r);
    auto mc = modPanel.content().translated (modPanel.getX(), modPanel.getY());
    auto monoArea = mc.removeFromRight (70);
    mono.setBounds (monoArea.removeFromTop (24).translated (0, 12));
    int mw = mc.getWidth() / 5;
    for (auto* k : { &lfoRate, &lfoPitch, &lfoCutoff, &glide, &tune }) k->setBounds (mc.removeFromLeft (mw).reduced (1));
    auto tc = texturePanel.content().translated (texturePanel.getX(), texturePanel.getY());
    int tw = tc.getWidth() / 4;
    for (auto* k : { &wobble, &lofi, &haze, &room }) k->setBounds (tc.removeFromLeft (tw).reduced (1));
    auto xc = fxPanel.content().translated (fxPanel.getX(), fxPanel.getY());
    meter.setBounds (xc.removeFromRight (20));
    xc.removeFromRight (6);
    int xw = xc.getWidth() / 5;
    for (auto* k : { &drive, &delayTime, &delayFb, &delayMix, &volume }) k->setBounds (xc.removeFromLeft (xw).reduced (1));
}
