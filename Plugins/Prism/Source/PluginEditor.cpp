#include "PluginEditor.h"

using namespace prism::ui;
static constexpr int kW = 1320, kH = 880;

PrismEditor::PrismEditor (PrismProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&look);

    addAndMakeVisible (logo);
    proc.presets.populate (presetBox);
    auto prevChange = presetBox.onChange;
    presetBox.onChange = [this, prevChange] { proc.undo.beginNewTransaction(); if (prevChange) prevChange(); };
    presetBox.setTextWhenNothingSelected ("Presets");
    addAndMakeVisible (presetBox);

    for (auto* b : { &abButton, &diceButton, &undoButton, &setA, &setB, &recButton, &playButton, &clearButton })
    {
        b->setColour (juce::TextButton::buttonOnColourId, Palette::ink());
        addAndMakeVisible (b);
    }
    abButton.setClickingTogglesState (true);
    abButton.setTooltip ("Compare two settings");
    abButton.onClick = [this] { ab.toggle (proc.apvts); };
    diceButton.setTooltip ("Randomise every unlocked module");
    diceButton.onClick = [this]
    {
        std::array<bool, 4> locks {};
        for (int i = 0; i < 4; ++i) locks[(size_t) i] = cards[i]->isLocked();
        proc.undo.beginNewTransaction();
        proc.randomise (locks);
    };
    undoButton.setTooltip ("Undo the last change");
    undoButton.onClick = [this] { proc.undo.undo(); };
    setA.setTooltip ("Store the current sound as scene A");
    setB.setTooltip ("Store the current sound as scene B");
    setA.onClick = [this] { proc.captureScene (0); flashA = 1; };
    setB.onClick = [this] { proc.captureScene (1); flashB = 1; };
    morph.setSliderStyle (juce::Slider::LinearHorizontal);
    morph.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    morph.setTooltip ("Morph between scene A and scene B (set both first)");
    morphAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, PID::morph, morph);
    addAndMakeVisible (morph);

    for (int m = 0; m < 4; ++m)
    {
        auto* c = cards.add (new ModuleCard (proc, m));
        c->onMove = [this] (int mod, int dir) { moveModule (mod, dir); };
        addAndMakeVisible (c);
    }

    addAndMakeVisible (pad);
    recButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff3b30));
    playButton.setColour (juce::TextButton::buttonOnColourId, Palette::module (2));
    recButton.setTooltip ("Record a pad gesture for one loop (follows the host transport)");
    recButton.onClick = [this] { proc.armGesture(); };
    playButton.onClick = [this] { proc.playGesture (proc.gestureState() != PrismProcessor::GestureState::Playing); };
    clearButton.onClick = [this] { proc.clearGesture(); };
    if (auto* lenParam = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (PID::gestureLen)))
        gestureLen.addItemList (lenParam->choices, 1);
    gestureLenAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, PID::gestureLen, gestureLen);
    addAndMakeVisible (gestureLen);
    addAndMakeVisible (padDepth);

    for (auto* k : { &input, &tilt, &width, &duck, &gmix, &output }) addAndMakeVisible (k);

    for (auto* t : { &sync, &safety }) { t->setColour (juce::ToggleButton::tickColourId, Palette::module (2)); addAndMakeVisible (t); }
    syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, PID::sync, sync);
    safetyAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, PID::safety, safety);
    freeze.caption = "FREEZE";
    freeze.setTooltip ("Hold the delay and reverb tails forever");
    freezeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, PID::freeze, freeze);
    addAndMakeVisible (freeze);
    addAndMakeVisible (scope);

    setResizable (true, true);
    setResizeLimits (kW * 3 / 4, kH * 3 / 4, kW * 3 / 2, kH * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) kW / kH);
    setSize (kW, kH);
    startTimerHz (30);
}

PrismEditor::~PrismEditor() { stopTimer(); setLookAndFeel (nullptr); }

juce::Rectangle<int> PrismEditor::cardArea (int pos) const
{
    const int gap = 40, x0 = 24, cw = (kW - 48 - 3 * gap) / 4;
    return { x0 + pos * (cw + gap), 88, cw, 500 };
}

void PrismEditor::layoutCards()
{
    shownOrder = proc.currentOrder();
    for (int pos = 0; pos < 4; ++pos)
    {
        auto* c = cards[shownOrder[(size_t) pos]];
        c->setBounds (cardArea (pos));
        c->setChainPosition (pos);
    }
}

void PrismEditor::moveModule (int module, int direction)
{
    auto order = proc.currentOrder();
    int pos = 0;
    for (int i = 0; i < 4; ++i) if (order[(size_t) i] == module) pos = i;
    const int np = juce::jlimit (0, 3, pos + direction);
    if (np == pos) return;
    std::swap (order[(size_t) pos], order[(size_t) np]);
    proc.undo.beginNewTransaction();
    setParam (proc.apvts, PID::order, (float) PrismProcessor::permutationIndex (order));
}

void PrismEditor::resized()
{
    // Everything is laid out on a fixed 1320x880 canvas and scaled, so it looks identical at any size
    const float s = (float) getWidth() / (float) kW;
    for (auto* c : getChildren()) if (dynamic_cast<juce::TooltipWindow*> (c) == nullptr) c->setTransform (juce::AffineTransform::scale (s));

    logo.setBounds (24, 14, 150, 56);
    auto top = juce::Rectangle<int> (330, 22, kW - 354, 40);
    abButton.setBounds (top.removeFromRight (56)); top.removeFromRight (8);
    presetBox.setBounds (top.removeFromRight (230)); top.removeFromRight (8);
    diceButton.setBounds (top.removeFromRight (66)); top.removeFromRight (8);
    undoButton.setBounds (top.removeFromRight (66)); top.removeFromRight (28);
    setB.setBounds (top.removeFromRight (66)); top.removeFromRight (10);
    morph.setBounds (top.removeFromRight (190)); top.removeFromRight (10);
    setA.setBounds (top.removeFromRight (66));

    layoutCards();

    deck = { 24, 608, kW - 48, 252 };
    auto d = deck.reduced (20, 16).withTrimmedTop (20);
    perform = d.removeFromLeft (440);
    d.removeFromLeft (20);
    outArea = d.removeFromRight (360);
    d.removeFromRight (20);
    master = d;

    auto pf = perform;
    pad.setBounds (pf.removeFromLeft (212).withHeight (212));
    pf.removeFromLeft (16);
    auto col = pf.removeFromLeft (100);
    recButton.setBounds (col.removeFromTop (30)); col.removeFromTop (8);
    playButton.setBounds (col.removeFromTop (30)); col.removeFromTop (8);
    clearButton.setBounds (col.removeFromTop (30)); col.removeFromTop (8);
    gestureLen.setBounds (col.removeFromTop (30));
    pf.removeFromLeft (8);
    padDepth.setBounds (pf.removeFromTop (110).reduced (0, 4));

    auto mr = master;
    auto knobRow = mr.removeFromTop (112);
    const int kw = knobRow.getWidth() / 6;
    for (auto* k : { &input, &tilt, &width, &duck, &gmix, &output }) k->setBounds (knobRow.removeFromLeft (kw).reduced (2, 0));
    mr.removeFromTop (12);
    auto toggles = mr.removeFromLeft (170);
    sync.setBounds (toggles.removeFromTop (30)); toggles.removeFromTop (8);
    safety.setBounds (toggles.removeFromTop (30));
    freeze.setBounds (mr.removeFromRight (120).withSizeKeepingCentre (76, 84).translated (0, -4));

    scope.setBounds (outArea.withHeight (212));
}

void PrismEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff2a2b2f));
    const float s = (float) getWidth() / (float) kW;
    g.addTransform (juce::AffineTransform::scale (s));

    // physically rendered faceplate (brushed aluminium, chamfered edge, machined wells), shared by every instance
    static juce::Image sharedChassis;
    if (! sharedChassis.isValid())
    {
        std::vector<prism::ui::pbr::Recess> wells;
        for (auto* c : { (juce::Component*) &setA, (juce::Component*) &setB, (juce::Component*) &undoButton, (juce::Component*) &diceButton, (juce::Component*) &abButton, (juce::Component*) &presetBox })
            wells.push_back ({ c->getBounds().toFloat().expanded (3.0f), 9.0f, 2.2f, 1.5f });
        wells.push_back ({ morph.getBounds().toFloat().withSizeKeepingCentre ((float) morph.getWidth() + 8, 18.0f), 9.0f, 2.0f, 1.5f });
        sharedChassis = prism::ui::pbr::renderChassis ((float) kW, (float) kH, 2.0f, wells);
    }
    g.setOpacity (1.0f);
    g.drawImage (sharedChassis, juce::Rectangle<float> (0, 0, (float) kW, (float) kH), juce::RectanglePlacement::stretchToFit);
    for (auto pt : { juce::Point<float> (16, 16), juce::Point<float> ((float) kW - 16, 16), juce::Point<float> (16, (float) kH - 16), juce::Point<float> ((float) kW - 16, (float) kH - 16) })
        prism::ui::pbr::drawScrew (g, pt, 7.0f);
    // the module plates and the deck stand proud of the faceplate: contact shadow + long soft key-light shadow
    auto castShadow = [&] (juce::Rectangle<float> r, float rad)
    {
        juce::Path p; p.addRoundedRectangle (r, rad);
        juce::DropShadow (juce::Colours::black.withAlpha (0.40f), 5, { 3, 1 }).drawForPath (g, p);
        juce::DropShadow (juce::Colours::black.withAlpha (0.26f), 30, { 22, 6 }).drawForPath (g, p);
    };
    for (int pos = 0; pos < 4; ++pos) castShadow (cardArea (pos).toFloat().reduced (3), 14.0f);
    castShadow (deck.toFloat(), 16.0f);

    // wordmark
    prism::ui::mat::engraved (g, "PRISM", { 180, 18, 220, 34 }, prism::ui::fonts::display (27.0f), Palette::ink(), juce::Justification::centredLeft);
    prism::ui::mat::engraved (g, juce::String::fromUTF8 ("FOUR-COLOUR EFFECT ENGINE  \xc2\xb7  POLAR AUDIO"), { 182, 50, 360, 16 }, prism::ui::fonts::label (13.0f), Palette::ink2(), juce::Justification::centredLeft);
    g.setColour (Palette::ink3()); g.setFont (prism::ui::fonts::label (12.0f));
    g.drawText ("A", setA.getRight() + 2, 28, 10, 28, juce::Justification::centredLeft);
    g.drawText ("B", setB.getX() - 12, 28, 10, 28, juce::Justification::centredRight);
    auto flash = [&] (juce::Rectangle<int> r, float f, bool has)
    {
        g.setColour ((has ? Palette::module (2) : Palette::line()).withAlpha (has ? 0.9f : 1.0f));
        g.fillEllipse ((float) r.getCentreX() - 3, (float) r.getBottom() + 4, 6, 6);
        if (f > 0) { g.setColour (Palette::module (2).withAlpha (f * 0.4f)); g.fillRoundedRectangle (r.toFloat().expanded (4), 18); }
    };
    flash (setA.getBounds(), flashA, proc.hasScene (0));
    flash (setB.getBounds(), flashB, proc.hasScene (1));

    // signal-flow arrows between the cards, with light pulses in the colour of the module that feeds them
    for (int pos = 0; pos < 3; ++pos)
    {
        auto a = cardArea (pos), b = cardArea (pos + 1);
        const float y = (float) a.getY() + 72.0f;
        const float x0 = (float) a.getRight() + 4, x1 = (float) b.getX() - 4;
        const auto col = Palette::module (shownOrder[(size_t) pos]);
        g.setColour (Palette::line().darker (0.05f)); g.drawLine (x0, y, x1 - 6, y, 2.0f);
        juce::Path head; head.addTriangle (x1 - 8, y - 5, x1, y, x1 - 8, y + 5);
        g.fillPath (head);
        for (int k = 0; k < 3; ++k)
        {
            const float ph = std::fmod (anim * 0.9f + (float) k / 3.0f, 1.0f);
            const float x = x0 + (x1 - x0 - 8) * ph;
            g.setColour (col.withAlpha (std::sin (ph * juce::MathConstants<float>::pi)));
            g.fillEllipse (x - 3, y - 3, 6, 6);
        }
    }
    // IN / OUT tags
    g.setColour (Palette::ink3()); g.setFont (prism::ui::fonts::label (12.0f));
    g.drawText ("IN", 4, cardArea (0).getY() + 64, 20, 16, juce::Justification::centred);
    g.drawText ("OUT", kW - 26, cardArea (3).getY() + 64, 24, 16, juce::Justification::centred);

    // performance deck
    if (! deckFace.isValid())
    {
        std::vector<prism::ui::pbr::Recess> wells;
        auto rel = [&] (juce::Rectangle<int> r, float grow) { return r.toFloat().expanded (grow).translated ((float) -deck.getX(), (float) -deck.getY()); };
        wells.push_back ({ rel (pad.getBounds(), 4.0f), 16.0f, 4.0f, 2.0f });
        wells.push_back ({ rel (scope.getBounds(), 4.0f), 14.0f, 4.0f, 2.0f });
        for (auto* b : { &recButton, &playButton, &clearButton }) wells.push_back ({ rel (b->getBounds(), -1.5f), 6.5f, 1.6f, 1.2f });
        wells.push_back ({ rel (gestureLen.getBounds(), 2.0f), 8.0f, 2.0f, 1.2f });
        deckFace = prism::ui::pbr::renderPlate ((float) deck.getX(), (float) deck.getY(), (float) deck.getWidth(), (float) deck.getHeight(), 2.0f, 0.0f, {}, wells, 16.0f, 99u);
    }
    g.setOpacity (1.0f);
    g.drawImage (deckFace, deck.toFloat(), juce::RectanglePlacement::stretchToFit);
    for (auto pt : { juce::Point<float> ((float) deck.getX() + 16, (float) deck.getY() + 16), juce::Point<float> ((float) deck.getRight() - 16, (float) deck.getY() + 16),
                     juce::Point<float> ((float) deck.getX() + 16, (float) deck.getBottom() - 16), juce::Point<float> ((float) deck.getRight() - 16, (float) deck.getBottom() - 16) })
        prism::ui::pbr::drawScrew (g, pt, 5.5f);
    const auto lf = prism::ui::fonts::display (9.5f);
    prism::ui::mat::engraved (g, juce::String::fromUTF8 ("PERFORM  \xc2\xb7  XY PAD + GESTURE"), juce::Rectangle<int> (perform.getX() + 14, deck.getY() + 12, 300, 16).toFloat(), lf, Palette::ink(), juce::Justification::centredLeft);
    prism::ui::mat::engraved (g, "MASTER", juce::Rectangle<int> (master.getX(), deck.getY() + 12, 200, 16).toFloat(), lf, Palette::ink(), juce::Justification::centredLeft);
    prism::ui::mat::engraved (g, "OUTPUT", juce::Rectangle<int> (outArea.getX(), deck.getY() + 12, 200, 16).toFloat(), lf, Palette::ink(), juce::Justification::centredLeft);
    for (int x : { perform.getRight() + 10, outArea.getX() - 10 })
    {
        g.setColour (juce::Colours::black.withAlpha (0.18f)); g.drawVerticalLine (x, (float) deck.getY() + 18, (float) deck.getBottom() - 18);
        g.setColour (juce::Colours::white.withAlpha (0.9f)); g.drawVerticalLine (x + 1, (float) deck.getY() + 18, (float) deck.getBottom() - 18);
    }
    // chain readout + gesture status
    auto order = shownOrder;
    static const char* names[4] = { "Color", "Motion", "Space", "Air" };
    const auto arrow = juce::String::fromUTF8 ("  \xe2\x86\x92  ");
    juce::String chain; for (int i = 0; i < 4; ++i) { chain << names[order[(size_t) i]]; if (i < 3) chain << arrow; }
    g.setColour (Palette::ink2()); g.setFont (prism::ui::fonts::label (12.6f));
    g.drawText (chain, master.getX(), deck.getY() + 12, master.getWidth(), 16, juce::Justification::centredRight);
    const char* gs[] = { "pad: live", "gesture: armed", "gesture: recording", "gesture: looping" };
    g.drawText (gs[(int) proc.gestureState()], recButton.getX(), pad.getBottom() - 16, 160, 16, juce::Justification::centredLeft);
    const float gp = proc.gesturePhase();
    auto bar = juce::Rectangle<float> ((float) recButton.getX(), (float) pad.getBottom() - 24, 100, 4);
    g.setColour (Palette::line()); g.fillRoundedRectangle (bar, 2);
    g.setColour (Palette::module (1)); g.fillRoundedRectangle (bar.withWidth (100 * gp), 2);
}

void PrismEditor::timerCallback()
{
    const float dt = 1.0f / 30.0f;
    anim += dt;
    flashA = juce::jmax (0.0f, flashA - dt * 2); flashB = juce::jmax (0.0f, flashB - dt * 2);
    if (proc.currentOrder() != shownOrder) layoutCards();
    for (auto* c : cards) c->tick (dt);
    pad.tick (dt); scope.tick(); logo.tick();
    const auto st = proc.gestureState();
    recButton.setToggleState (st == PrismProcessor::GestureState::Armed || st == PrismProcessor::GestureState::Recording, juce::dontSendNotification);
    playButton.setToggleState (st == PrismProcessor::GestureState::Playing, juce::dontSendNotification);
    // only the animated bits of the editor's own paint: flow arrows, scene flashes, gesture status
    const float sc = (float) getWidth() / (float) kW;
    auto dirty = [&] (juce::Rectangle<int> r) { repaint (r.toFloat().transformedBy (juce::AffineTransform::scale (sc)).getSmallestIntegerContainer().expanded (2)); };
    for (int pos = 0; pos < 3; ++pos) dirty ({ cardArea (pos).getRight(), cardArea (pos).getY() + 60, 40, 24 });
    dirty (setA.getBounds().expanded (8).withBottom (setA.getBottom() + 14));
    dirty (setB.getBounds().expanded (8).withBottom (setB.getBottom() + 14));
    dirty ({ recButton.getX(), pad.getBottom() - 26, 170, 28 });
}
