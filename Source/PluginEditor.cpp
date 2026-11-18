#include "PluginEditor.h"
#include "gui/Theme.h"

using namespace arroyo;

// ================================================================ look & feel
ArroyoLook::ArroyoLook()
{
    setColour (juce::ComboBox::backgroundColourId, theme::panel2);
    setColour (juce::ComboBox::outlineColourId, theme::grid);
    setColour (juce::ComboBox::textColourId, theme::text);
    setColour (juce::ComboBox::arrowColourId, theme::dim);
    setColour (juce::PopupMenu::backgroundColourId, theme::panel2);
    setColour (juce::PopupMenu::textColourId, theme::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::headerTextColourId, theme::dim);
    setColour (juce::Slider::textBoxTextColourId, theme::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, theme::accent.withAlpha (0.4f));
    setColour (juce::Label::textColourId, theme::text);
    setColour (juce::TextEditor::backgroundColourId, theme::panel2);
    setColour (juce::TextEditor::textColourId, theme::text);
    setColour (juce::TextEditor::highlightColourId, theme::accent.withAlpha (0.4f));
    setColour (juce::TextButton::buttonColourId, theme::panel2);
    setColour (juce::TextButton::textColourOffId, theme::dim);
    setColour (juce::TextButton::textColourOnId, theme::bg);
    setColour (juce::CaretComponent::caretColourId, theme::accent);
}

juce::Font ArroyoLook::getComboBoxFont (juce::ComboBox&) { return theme::font (13.0f); }
juce::Font ArroyoLook::getPopupMenuFont() { return theme::font (14.0f); }

juce::Label* ArroyoLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (theme::font (12.5f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void ArroyoLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                   float start, float end, juce::Slider& s)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    auto colour = s.findColour (juce::Slider::rotarySliderFillColourId);
    if (! s.isEnabled())
        colour = theme::grid;

    g.setColour (theme::bg);
    g.fillEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (c).reduced (radius * 0.18f));

    juce::Path track;
    track.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, start, end, true);
    g.setColour (theme::grid);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // bipolar knobs (range centred on zero) fill from the middle
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? start + (end - start) * (float) s.valueToProportionOfLength (0.0) : start;
    juce::Path val;
    val.addCentredArc (c.x, c.y, radius - 2.0f, radius - 2.0f, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (colour);
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path ptr;
    const float pl = radius * 0.55f;
    ptr.addRoundedRectangle (-1.5f, -pl, 3.0f, pl * 0.65f, 1.5f);
    g.setColour (s.isEnabled() ? theme::text : theme::dim);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c));
}

void ArroyoLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                   juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto r = juce::Rectangle<int> (x, y, w, h).toFloat();
    const float cy = r.getCentreY();
    g.setColour (theme::grid);
    g.fillRoundedRectangle (r.getX(), cy - 2.0f, r.getWidth(), 4.0f, 2.0f);
    g.setColour (s.findColour (juce::Slider::trackColourId));
    const float zero = r.getX() + r.getWidth() * (float) s.valueToProportionOfLength (0.0);
    g.fillRoundedRectangle (juce::jmin (zero, pos), cy - 2.0f, std::abs (pos - zero), 4.0f, 2.0f);
    g.setColour (theme::text);
    g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ pos, cy }));
}

void ArroyoLook::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hi, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const auto c = b.findColour (juce::ToggleButton::tickColourId);
    g.setColour (on ? c.withAlpha (0.9f) : theme::panel2.brighter (hi ? 0.15f : 0.0f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (on ? c : theme::grid.brighter (0.2f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    g.setColour (on ? theme::bg : theme::dim);
    g.setFont (theme::font (12.5f, true));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

// ================================================================ widgets
Knob::Knob (const juce::String& cap, juce::Colour c) : caption (cap), colour (c.isTransparent() ? theme::accent : c)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, theme::text);
    addAndMakeVisible (slider);
}

void Knob::resized()
{
    slider.setBounds (getLocalBounds().withTrimmedTop (15));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (theme::dim);
    g.setFont (theme::font (11.0f, true));
    g.drawText (caption, getLocalBounds().withHeight (14), juce::Justification::centred);
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto text = r.removeFromBottom (14);
    g.setColour (theme::dim);
    g.setFont (theme::font (10.5f, true));
    g.drawText (label, text, juce::Justification::centred);
    r = r.reduced (2.0f, 0);
    g.setColour (theme::bg);
    g.fillRoundedRectangle (r, 2.0f);

    if (isGr)
    {
        const float frac = juce::jlimit (0.0f, 1.0f, level / 24.0f);
        g.setColour (theme::accent);
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * frac), 2.0f);
        g.setColour (theme::text);
        g.setFont (theme::font (10.0f));
        g.drawText (level > 0.05f ? "-" + juce::String (level, 1) : "0", r.withHeight (14).translated (0, r.getHeight() - 16),
                    juce::Justification::centred);
        return;
    }
    // -60 .. +6 dBFS
    const float frac = juce::jlimit (0.0f, 1.0f, (level + 60.0f) / 66.0f);
    auto bar = r.withTrimmedTop (r.getHeight() * (1.0f - frac));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9534f), 0, r.getY(), theme::accent2, 0, r.getBottom(), false));
    g.fillRoundedRectangle (bar, 2.0f);
    const float zeroY = r.getY() + r.getHeight() * (6.0f / 66.0f);
    g.setColour (theme::text.withAlpha (0.5f));
    g.drawHorizontalLine ((int) zeroY, r.getX(), r.getRight());
}

// ================================================================ editor
static void styleCombo (juce::ComboBox& c, const juce::StringArray& items)
{
    c.addItemList (items, 1);
}

ArroyoEditor::ArroyoEditor (ArroyoProcessor& p)
    : AudioProcessorEditor (p), proc (p), curve (p, [this] (int b) { selectBand (b); })
{
    setLookAndFeel (&look);
    addAndMakeVisible (curve);

    // ---- top bar
    eqOn.setColour (juce::ToggleButton::tickColourId, theme::accent);
    addAndMakeVisible (eqOn);
    eqOnAtt = std::make_unique<BA> (proc.apvts, "eq_on", eqOn);

    auto setupProp = [this] (juce::ComboBox& box, juce::Label& lbl, const juce::String& name,
                             const juce::StringArray& items, const juce::Identifier& prop)
    {
        styleCombo (box, items);
        box.setSelectedItemIndex ((int) proc.apvts.state.getProperty (prop, 0), juce::dontSendNotification);
        box.onChange = [this, &box, prop] { proc.apvts.state.setProperty (prop, box.getSelectedItemIndex(), nullptr); curve.repaint(); };
        lbl.setText (name, juce::dontSendNotification);
        lbl.setFont (theme::font (11.0f, true));
        lbl.setColour (juce::Label::textColourId, theme::dim);
        lbl.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (box);
        addAndMakeVisible (lbl);
    };
    setupProp (analyzerBox, analyzerLbl, "ANALYZER", { "Off", "Input", "Output", "Input + Output" }, "analyzer");
    setupProp (rangeBox, rangeLbl, "RANGE", { "12 dB", "24 dB", "36 dB" }, "range");
    setupProp (tiltBox, tiltLbl, "TILT", { "0 dB/oct", "3 dB/oct", "4.5 dB/oct", "6 dB/oct" }, "tilt");

    // ---- band panel
    for (int b = 0; b < kNumBands; ++b)
    {
        auto* btn = bandButtons.add (new juce::TextButton (juce::String (b + 1)));
        btn->setClickingTogglesState (false);
        btn->onClick = [this, b] { selectBand (b); };
        addAndMakeVisible (btn);
    }
    bandOn.setColour (juce::ToggleButton::tickColourId, theme::accent);
    addAndMakeVisible (bandOn);
    styleCombo (typeBox, bandTypeNames());
    styleCombo (slopeBox, slopeNames());
    addAndMakeVisible (typeBox);
    addAndMakeVisible (slopeBox);
    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
        addAndMakeVisible (k);

    // ---- compressor
    compOn.setColour (juce::ToggleButton::tickColourId, theme::accent2);
    autoMakeup.setColour (juce::ToggleButton::tickColourId, theme::accent2);
    link.setColour (juce::ToggleButton::tickColourId, theme::accent2);
    for (auto* b : { &compOn, &autoMakeup, &link })
        addAndMakeVisible (b);
    styleCombo (detectorBox, { "Peak", "RMS" });
    styleCombo (positionBox, { "After EQ", "Before EQ" });
    addAndMakeVisible (detectorBox);
    addAndMakeVisible (positionBox);
    const std::pair<Knob*, const char*> compKnobs[] = { { &thresh, "comp_thresh" }, { &ratio, "comp_ratio" }, { &knee, "comp_knee" },
                                                        { &attack, "comp_attack" }, { &release, "comp_release" }, { &makeup, "comp_makeup" },
                                                        { &mix, "comp_mix" }, { &scHpf, "comp_schpf" } };
    for (auto& [k, id] : compKnobs)
    {
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, theme::accent2);
        addAndMakeVisible (k);
        compSliderAtts.push_back (std::make_unique<SA> (proc.apvts, id, k->slider));
    }
    compButtonAtts.push_back (std::make_unique<BA> (proc.apvts, "comp_on", compOn));
    compButtonAtts.push_back (std::make_unique<BA> (proc.apvts, "comp_auto", autoMakeup));
    compButtonAtts.push_back (std::make_unique<BA> (proc.apvts, "comp_link", link));
    compComboAtts.push_back (std::make_unique<CA> (proc.apvts, "comp_detector", detectorBox));
    compComboAtts.push_back (std::make_unique<CA> (proc.apvts, "comp_position", positionBox));
    addAndMakeVisible (grMeter);

    // ---- output
    outKnob.slider.setColour (juce::Slider::rotarySliderFillColourId, theme::text);
    addAndMakeVisible (outKnob);
    outAtt = std::make_unique<SA> (proc.apvts, "out_gain", outKnob.slider);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    selectBand (0);
    for (int b = 0; b < kNumBands; ++b)
        if (proc.getBandSpec (b).on) { selectBand (b); break; }

    setResizable (true, true);
    setResizeLimits (960, 620, 1920, 1240);
    setSize (1160, 740);
    startTimerHz (30);
}

ArroyoEditor::~ArroyoEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ArroyoEditor::attachBand (int b)
{
    freqAtt.reset(); gainAtt.reset(); qAtt.reset(); bandOnAtt.reset(); typeAtt.reset(); slopeAtt.reset();
    const auto c = theme::band (b);
    for (auto* k : { &freqKnob, &gainKnob, &qKnob })
        k->slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    bandOn.setColour (juce::ToggleButton::tickColourId, c);
    freqAtt = std::make_unique<SA> (proc.apvts, ids::band (b, "freq"), freqKnob.slider);
    gainAtt = std::make_unique<SA> (proc.apvts, ids::band (b, "gain"), gainKnob.slider);
    qAtt = std::make_unique<SA> (proc.apvts, ids::band (b, "q"), qKnob.slider);
    bandOnAtt = std::make_unique<BA> (proc.apvts, ids::band (b, "on"), bandOn);
    typeAtt = std::make_unique<CA> (proc.apvts, ids::band (b, "type"), typeBox);
    slopeAtt = std::make_unique<CA> (proc.apvts, ids::band (b, "slope"), slopeBox);
}

void ArroyoEditor::selectBand (int b)
{
    selectedBand = b;
    curve.setSelected (b);
    attachBand (b);
    repaint();
}

void ArroyoEditor::timerCallback()
{
    curve.updateAnalyzer();

    // band button states and context-sensitive controls
    for (int b = 0; b < kNumBands; ++b)
    {
        const bool on = proc.getBandSpec (b).on;
        auto* btn = bandButtons[b];
        const auto c = theme::band (b);
        btn->setColour (juce::TextButton::buttonColourId, on ? c.withAlpha (0.85f) : theme::panel2);
        btn->setColour (juce::TextButton::textColourOffId, on ? theme::bg : c.withAlpha (0.8f));
        btn->setColour (juce::ComboBox::outlineColourId, b == selectedBand ? juce::Colours::white : theme::grid);
    }
    const auto s = proc.getBandSpec (selectedBand);
    gainKnob.slider.setEnabled (typeHasGain (s.type));
    slopeBox.setEnabled (typeHasSlope (s.type));

    // meters: ballistics here, the audio thread only stores peaks
    auto toDb = [] (float v) { return juce::Decibels::gainToDecibels (v, -100.0f); };
    const float in = toDb (proc.inPeak.exchange (0.0f)), out = toDb (proc.outPeak.exchange (0.0f));
    inLevel = std::max (in, inLevel - 1.2f);
    outLevel = std::max (out, outLevel - 1.2f);
    const float gr = proc.gainReduction().load();
    grLevel = gr > grLevel ? gr : std::max (gr, grLevel - 0.6f);
    inMeter.setLevel (inLevel);
    outMeter.setLevel (outLevel);
    grMeter.setLevel (grLevel);
}

void ArroyoEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // title
    auto t = topArea.reduced (14, 0);
    g.setColour (theme::accent);
    g.setFont (theme::font (21.0f, true));
    g.drawText ("ARROYO", t.removeFromLeft (100), juce::Justification::centredLeft);
    g.setColour (theme::dim);
    g.setFont (theme::font (13.0f));
    g.drawText ("16-band EQ  +  compressor", t.removeFromLeft (220), juce::Justification::centredLeft);

    for (auto area : { bandArea, compArea, outArea })
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (area.toFloat(), 6.0f);
    }

    g.setFont (theme::font (12.0f, true));
    g.setColour (theme::band (selectedBand));
    g.drawText ("BAND " + juce::String (selectedBand + 1), bandArea.reduced (12, 8).removeFromTop (16), juce::Justification::centredLeft);
    g.setColour (theme::dim);
    g.setFont (theme::font (11.0f));
    g.drawText ("Double-click graph: add band   Drag: move   Shift: fine   Wheel: Q   Right-click: type",
                bandArea.reduced (12, 8).removeFromTop (16), juce::Justification::centredRight);

    g.setFont (theme::font (12.0f, true));
    g.setColour (theme::accent2);
    g.drawText ("COMPRESSOR", compArea.reduced (12, 8).removeFromTop (16), juce::Justification::centredLeft);
}

void ArroyoEditor::resized()
{
    auto r = getLocalBounds();
    topArea = r.removeFromTop (44);
    const int bottomH = juce::jlimit (230, 300, getHeight() / 3);
    auto bottom = r.removeFromBottom (bottomH).reduced (10, 8);
    curve.setBounds (r.reduced (10, 0));

    // top bar (right to left)
    auto t = topArea.reduced (12, 9);
    eqOn.setBounds (t.removeFromRight (56));
    t.removeFromRight (16);
    tiltBox.setBounds (t.removeFromRight (110)); tiltLbl.setBounds (t.removeFromRight (44));
    t.removeFromRight (8);
    rangeBox.setBounds (t.removeFromRight (84)); rangeLbl.setBounds (t.removeFromRight (52));
    t.removeFromRight (8);
    analyzerBox.setBounds (t.removeFromRight (136)); analyzerLbl.setBounds (t.removeFromRight (76));

    // bottom: band | compressor | output
    outArea = bottom.removeFromRight (120);
    bottom.removeFromRight (8);
    compArea = bottom.removeFromRight ((int) (bottom.getWidth() * 0.47f));
    bottom.removeFromRight (8);
    bandArea = bottom;

    // band panel
    auto b = bandArea.reduced (12, 8);
    b.removeFromTop (22);
    auto row = b.removeFromTop (24);
    const int bw = juce::jmin (32, row.getWidth() / kNumBands);
    for (int i = 0; i < kNumBands; ++i)
        bandButtons[i]->setBounds (row.removeFromLeft (bw).reduced (1, 0));
    b.removeFromTop (10);
    auto controls = b.removeFromLeft (130);
    bandOn.setBounds (controls.removeFromTop (26).withWidth (64));
    controls.removeFromTop (10);
    typeBox.setBounds (controls.removeFromTop (26));
    controls.removeFromTop (8);
    slopeBox.setBounds (controls.removeFromTop (26));
    b.removeFromLeft (10);
    const int kw = b.getWidth() / 3;
    auto capped = [] (juce::Rectangle<int> a) { return a.withSizeKeepingCentre (juce::jmin (a.getWidth(), 112), juce::jmin (a.getHeight(), 128)); };
    freqKnob.setBounds (capped (b.removeFromLeft (kw)));
    gainKnob.setBounds (capped (b.removeFromLeft (kw)));
    qKnob.setBounds (capped (b));

    // compressor panel
    auto c = compArea.reduced (12, 8);
    auto head = c.removeFromTop (22);
    compOn.setBounds (head.removeFromRight (56).reduced (0, 1));
    grMeter.setBounds (c.removeFromRight (30));
    c.removeFromRight (8);
    auto opts = c.removeFromBottom (26);
    const int ow = opts.getWidth() / 4;
    detectorBox.setBounds (opts.removeFromLeft (ow).reduced (2, 0));
    positionBox.setBounds (opts.removeFromLeft (ow).reduced (2, 0));
    autoMakeup.setBounds (opts.removeFromLeft (ow).reduced (2, 0));
    link.setBounds (opts.reduced (2, 0));
    c.removeFromBottom (6);
    auto r1 = c.removeFromTop (c.getHeight() / 2);
    const int cw = r1.getWidth() / 4;
    for (auto* k : { &thresh, &ratio, &knee, &attack })
        k->setBounds (r1.removeFromLeft (cw));
    for (auto* k : { &release, &makeup, &mix, &scHpf })
        k->setBounds (c.removeFromLeft (cw));

    // output panel
    auto o = outArea.reduced (10, 10);
    outKnob.setBounds (o.removeFromTop (92));
    o.removeFromTop (6);
    const int mw = o.getWidth() / 2;
    inMeter.setBounds (o.removeFromLeft (mw).reduced (6, 0));
    outMeter.setBounds (o.reduced (6, 0));
}
