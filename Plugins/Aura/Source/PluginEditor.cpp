#include "PluginEditor.h"

AuraEditor::AuraEditor (AuraProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (backdrop);
    addAndMakeVisible (header);
    for (auto* c : { &spacePanel, &tonePanel, &motionPanel, &outPanel }) addAndMakeVisible (c);
    addAndMakeVisible (visual);
    for (auto* k : { &size, &decay, &predelay, &diffusion, &damp, &lowcut, &modRate, &modDepth, &shimmer, &width, &mix, &duck, &output })
        addAndMakeVisible (k);
    addAndMakeVisible (colour); addAndMakeVisible (freeze); addAndMakeVisible (meter);
    proc.presets.populate (header.presetBox);
    header.abButton.onClick = [this] { ab.toggle (proc.apvts); };
    setResizable (true, true);
    setResizeLimits (820, 480, 1400, 820);
    getConstrainer()->setFixedAspectRatio (940.0 / 560.0);
    setSize (940, 560);
}

AuraEditor::~AuraEditor() { setLookAndFeel (nullptr); }

void AuraEditor::resized()
{
    auto r = getLocalBounds();
    backdrop.setBounds (r);
    header.setBounds (r.removeFromTop (56));
    r.reduce (14, 12);
    auto top = r.removeFromTop ((int) (r.getHeight() * 0.56f));
    auto visualArea = top.removeFromLeft ((int) (top.getWidth() * 0.3f));
    visual.setBounds (visualArea);
    top.removeFromLeft (10);
    spacePanel.setBounds (top);
    auto grid = spacePanel.content();
    int kw = grid.getWidth() / 4;
    for (auto* k : { &size, &decay, &predelay, &diffusion }) { k->setBounds (grid.removeFromLeft (kw).reduced (6)); }
    spacePanel.setBounds (spacePanel.getBounds()); // keep
    // reposition knobs relative to editor (panels don't own children)
    for (auto* k : { &size, &decay, &predelay, &diffusion }) k->setBounds (k->getBounds() + spacePanel.getPosition());

    r.removeFromTop (10);
    auto bottom = r;
    int w3 = (bottom.getWidth() - 20) / 3;
    tonePanel.setBounds (bottom.removeFromLeft (w3)); bottom.removeFromLeft (10);
    motionPanel.setBounds (bottom.removeFromLeft (w3)); bottom.removeFromLeft (10);
    outPanel.setBounds (bottom);

    auto tc = tonePanel.content().translated (tonePanel.getX(), tonePanel.getY());
    auto choiceRow = tc.removeFromTop (48);
    colour.setBounds (choiceRow.reduced (6, 0));
    int tw = tc.getWidth() / 2;
    damp.setBounds (tc.removeFromLeft (tw).reduced (4));
    lowcut.setBounds (tc.reduced (4));

    auto mc = motionPanel.content().translated (motionPanel.getX(), motionPanel.getY());
    int mw = mc.getWidth() / 3;
    modRate.setBounds (mc.removeFromLeft (mw).reduced (4));
    modDepth.setBounds (mc.removeFromLeft (mw).reduced (4));
    shimmer.setBounds (mc.reduced (4));

    auto oc = outPanel.content().translated (outPanel.getX(), outPanel.getY());
    auto right = oc.removeFromRight (70);
    freeze.setBounds (right.removeFromTop (24));
    right.removeFromTop (6);
    meter.setBounds (right.reduced (22, 0));
    int ow = oc.getWidth() / 4;
    width.setBounds (oc.removeFromLeft (ow).reduced (3));
    mix.setBounds (oc.removeFromLeft (ow).reduced (3));
    duck.setBounds (oc.removeFromLeft (ow).reduced (3));
    output.setBounds (oc.reduced (3));
}
