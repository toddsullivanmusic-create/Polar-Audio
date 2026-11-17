#include "PluginEditor.h"

McGeeEditor::McGeeEditor (McGeeProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (backdrop); addAndMakeVisible (header);
    for (auto* c : { &oscPanel, &filterPanel, &envPanel, &modPanel, &fxPanel }) addAndMakeVisible (c);
    addAndMakeVisible (scope);
    for (auto* c : { &w1, &w2, &w3, &o2, &o3 }) addAndMakeVisible (c);
    for (auto* k : { &l1, &l2, &l3, &d2, &d3, &pw, &fat, &cutoff, &reso, &envAmt, &keyTrack, &fDrive, &fA, &fD, &fS, &fR, &aA, &aD, &aS, &aR,
                     &lfoRate, &lfoPitch, &lfoCutoff, &glide, &drive, &chorus, &delayTime, &delayFb, &delayMix, &volume })
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
    setResizeLimits (980, 600, 1600, 980);
    getConstrainer()->setFixedAspectRatio (1080.0 / 660.0);
    setSize (1080, 660);
}

McGeeEditor::~McGeeEditor() { setLookAndFeel (nullptr); }

void McGeeEditor::resized()
{
    auto r = getLocalBounds();
    backdrop.setBounds (r);
    header.setBounds (r.removeFromTop (56));
    keyboard.setBounds (r.removeFromBottom (64).reduced (14, 6));
    r.reduce (14, 10);
    const int gap = 10;

    // Row 1: oscillators (wide) + scope
    auto row1 = r.removeFromTop ((int) (r.getHeight() * 0.42f));
    scope.setBounds (row1.removeFromRight ((int) (row1.getWidth() * 0.22f)));
    row1.removeFromRight (gap);
    oscPanel.setBounds (row1);
    auto oc = oscPanel.content().translated (oscPanel.getX(), oscPanel.getY());
    int colW = oc.getWidth() / 3;
    auto col = [&] (lumen::Choice& wave, lumen::Choice* oct, lumen::Knob& lvl, lumen::Knob* det, lumen::Knob* extra)
    {
        auto c = oc.removeFromLeft (colW).reduced (4, 0);
        auto top = c.removeFromTop (46);
        if (oct) { oct->setBounds (top.removeFromRight (64)); top.removeFromRight (6); }
        wave.setBounds (top);
        c.removeFromTop (4);
        int n = 1 + (det != nullptr) + (extra != nullptr);
        int kw = c.getWidth() / n;
        lvl.setBounds (c.removeFromLeft (kw));
        if (det) det->setBounds (c.removeFromLeft (kw));
        if (extra) extra->setBounds (c.removeFromLeft (kw));
    };
    col (w1, nullptr, l1, &pw, &fat);
    col (w2, &o2, l2, &d2, nullptr);
    col (w3, &o3, l3, &d3, nullptr);

    r.removeFromTop (gap);
    // Row 2: filter | envelopes
    auto row2 = r.removeFromTop ((int) (r.getHeight() * 0.5f));
    filterPanel.setBounds (row2.removeFromLeft ((int) (row2.getWidth() * 0.42f)));
    row2.removeFromLeft (gap);
    envPanel.setBounds (row2);
    auto fc = filterPanel.content().translated (filterPanel.getX(), filterPanel.getY());
    auto big = fc.removeFromLeft ((int) (fc.getWidth() * 0.3f));
    cutoff.setBounds (big);
    int fw = fc.getWidth() / 4;
    for (auto* k : { &reso, &envAmt, &keyTrack, &fDrive }) k->setBounds (fc.removeFromLeft (fw).reduced (2));
    auto ec = envPanel.content().translated (envPanel.getX(), envPanel.getY());
    auto filt = ec.removeFromLeft (ec.getWidth() / 2);
    int ew = filt.getWidth() / 4;
    for (auto* k : { &fA, &fD, &fS, &fR }) k->setBounds (filt.removeFromLeft (ew).reduced (2));
    for (auto* k : { &aA, &aD, &aS, &aR }) k->setBounds (ec.removeFromLeft (ew).reduced (2));

    r.removeFromTop (gap);
    // Row 3: modulation | effects
    modPanel.setBounds (r.removeFromLeft ((int) (r.getWidth() * 0.42f)));
    r.removeFromLeft (gap);
    fxPanel.setBounds (r);
    auto mc = modPanel.content().translated (modPanel.getX(), modPanel.getY());
    auto monoArea = mc.removeFromRight (80);
    mono.setBounds (monoArea.removeFromTop (24).translated (0, 10));
    int mw = mc.getWidth() / 4;
    for (auto* k : { &lfoRate, &lfoPitch, &lfoCutoff, &glide }) k->setBounds (mc.removeFromLeft (mw).reduced (2));
    auto xc = fxPanel.content().translated (fxPanel.getX(), fxPanel.getY());
    meter.setBounds (xc.removeFromRight (22));
    xc.removeFromRight (6);
    int xw = xc.getWidth() / 6;
    for (auto* k : { &drive, &chorus, &delayTime, &delayFb, &delayMix, &volume }) k->setBounds (xc.removeFromLeft (xw).reduced (2));
}
