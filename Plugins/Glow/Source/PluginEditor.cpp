#include "PluginEditor.h"

GlowEditor::GlowEditor (GlowProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (backdrop); addAndMakeVisible (header);
    for (auto* c : { &circuitPanel, &shapePanel, &outPanel }) addAndMakeVisible (c);
    addAndMakeVisible (scope);
    for (auto* k : { &drive, &bias, &focus, &dynamics, &tone, &mix, &output }) addAndMakeVisible (k);
    addAndMakeVisible (circuit); addAndMakeVisible (oversample); addAndMakeVisible (autogain); addAndMakeVisible (meter);
    proc.presets.populate (header.presetBox);
    header.abButton.onClick = [this] { ab.toggle (proc.apvts); };
    setResizable (true, true);
    setResizeLimits (760, 440, 1300, 760);
    getConstrainer()->setFixedAspectRatio (880.0 / 520.0);
    setSize (880, 520);
}

GlowEditor::~GlowEditor() { setLookAndFeel (nullptr); }

void GlowEditor::resized()
{
    auto r = getLocalBounds();
    backdrop.setBounds (r);
    header.setBounds (r.removeFromTop (56));
    r.reduce (14, 12);

    auto left = r.removeFromLeft ((int) (r.getWidth() * 0.34f));
    scope.setBounds (left.removeFromTop ((int) (left.getHeight() * 0.62f)));
    left.removeFromTop (10);
    circuitPanel.setBounds (left);
    auto cc = circuitPanel.content().translated (circuitPanel.getX(), circuitPanel.getY());
    circuit.setBounds (cc.removeFromTop (46).reduced (4, 0));
    cc.removeFromTop (6);
    oversample.setBounds (cc.removeFromTop (46).reduced (4, 0));

    r.removeFromLeft (10);
    shapePanel.setBounds (r.removeFromTop ((int) (r.getHeight() * 0.58f)));
    auto sc = shapePanel.content().translated (shapePanel.getX(), shapePanel.getY());
    auto big = sc.removeFromLeft ((int) (sc.getWidth() * 0.36f));
    drive.setBounds (big.reduced (4));
    int kw = sc.getWidth() / 3;
    bias.setBounds (sc.removeFromLeft (kw).reduced (6));
    focus.setBounds (sc.removeFromLeft (kw).reduced (6));
    dynamics.setBounds (sc.reduced (6));

    r.removeFromTop (10);
    outPanel.setBounds (r);
    auto oc = outPanel.content().translated (outPanel.getX(), outPanel.getY());
    auto right = oc.removeFromRight (110);
    autogain.setBounds (right.removeFromTop (24));
    right.removeFromTop (6);
    meter.setBounds (right.reduced (40, 0));
    int ow = oc.getWidth() / 3;
    tone.setBounds (oc.removeFromLeft (ow).reduced (3));
    mix.setBounds (oc.removeFromLeft (ow).reduced (3));
    output.setBounds (oc.reduced (3));
}
