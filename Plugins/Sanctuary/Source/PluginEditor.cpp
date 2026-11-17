#include "PluginEditor.h"

SanctuaryEditor::SanctuaryEditor (SanctuaryProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (backdrop); addAndMakeVisible (header);
    for (auto* c : { &soundPanel, &livePanel, &spacePanel }) addAndMakeVisible (c);
    addAndMakeVisible (wheel);
    for (auto* c : { &mode, &style, &motionBars }) addAndMakeVisible (c);
    for (auto* k : { &swell, &air, &depth, &colour, &motion, &transition, &fade, &reverb, &width, &output }) addAndMakeVisible (k);
    addAndMakeVisible (midiKeys); addAndMakeVisible (modWheel); addAndMakeVisible (meter);
    midiHelp.setText ("MIDI:  C1-B1 major keys  ·  C2-B2 minor keys  ·  C3 play/stop  ·  D3 fade  ·  mod wheel = swell  ·  program change 0-23", juce::dontSendNotification);
    midiHelp.setFont (lumen::uiFont (11.0f)); midiHelp.setColour (juce::Label::textColourId, theme.textFaint);
    midiHelp.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (midiHelp);
    proc.presets.populate (header.presetBox);
    header.abButton.onClick = [this] { ab.toggle (proc.apvts); };
    setResizable (true, true);
    setResizeLimits (900, 560, 1500, 930);
    getConstrainer()->setFixedAspectRatio (1000.0 / 620.0);
    setSize (1000, 620);
}

SanctuaryEditor::~SanctuaryEditor() { setLookAndFeel (nullptr); }

void SanctuaryEditor::paint (juce::Graphics&) {}

void SanctuaryEditor::resized()
{
    auto r = getLocalBounds();
    backdrop.setBounds (r);
    header.setBounds (r.removeFromTop (56));
    midiHelp.setBounds (r.removeFromBottom (22));
    r.reduce (14, 10);
    const int gap = 10;

    auto left = r.removeFromLeft ((int) (r.getWidth() * 0.4f));
    wheel.setBounds (left.removeFromTop ((int) (left.getHeight() * 0.72f)));
    left.removeFromTop (gap);
    livePanel.setBounds (left);
    auto lc = livePanel.content().translated (livePanel.getX(), livePanel.getY());
    auto toggles = lc.removeFromRight ((int) (lc.getWidth() * 0.45f));
    midiKeys.setBounds (toggles.removeFromTop (24).translated (0, 8));
    modWheel.setBounds (toggles.removeFromTop (24).translated (0, 14));
    int lw = lc.getWidth() / 2;
    transition.setBounds (lc.removeFromLeft (lw).reduced (2));
    fade.setBounds (lc.reduced (2));

    r.removeFromLeft (gap);
    soundPanel.setBounds (r.removeFromTop ((int) (r.getHeight() * 0.6f)));
    auto sc = soundPanel.content().translated (soundPanel.getX(), soundPanel.getY());
    auto top = sc.removeFromTop (46);
    int cw = (top.getWidth() - 12) / 3;
    mode.setBounds (top.removeFromLeft (cw)); top.removeFromLeft (6);
    style.setBounds (top.removeFromLeft (cw)); top.removeFromLeft (6);
    motionBars.setBounds (top);
    sc.removeFromTop (6);
    auto big = sc.removeFromLeft ((int) (sc.getWidth() * 0.3f));
    swell.setBounds (big.reduced (2));
    int kw = sc.getWidth() / 4;
    for (auto* k : { &air, &depth, &colour, &motion }) k->setBounds (sc.removeFromLeft (kw).reduced (2));

    r.removeFromTop (gap);
    spacePanel.setBounds (r);
    auto pc = spacePanel.content().translated (spacePanel.getX(), spacePanel.getY());
    meter.setBounds (pc.removeFromRight (22)); pc.removeFromRight (10);
    int pw = pc.getWidth() / 3;
    reverb.setBounds (pc.removeFromLeft (pw).reduced (2));
    width.setBounds (pc.removeFromLeft (pw).reduced (2));
    output.setBounds (pc.reduced (2));
}
