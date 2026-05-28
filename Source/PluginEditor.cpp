#include "PluginEditor.h"

namespace
{
    const juce::Colour kBgDeep    { 0xff0f1422 };
    const juce::Colour kBgPanel   { 0xff1b2a4a };
    const juce::Colour kBgPanelHi { 0xff253760 };
    const juce::Colour kAccent    { 0xffc9a227 };
    const juce::Colour kText      { 0xfff2e6c8 };
    const juce::Colour kTextDim   { 0xaaf2e6c8 };
    const juce::Colour kDanger    { 0xffd24b3e };

    void styleKnob (juce::Slider& s, juce::Component& parent)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 54, 14);
        s.setColour (juce::Slider::textBoxTextColourId, kText);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        parent.addAndMakeVisible (s);
    }

    void styleLabel (juce::Label& l, const juce::String& t, juce::Component& parent,
                     float size = 11.0f, bool bold = false,
                     juce::Justification just = juce::Justification::centred)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::Font (size, bold ? juce::Font::bold : juce::Font::plain));
        l.setColour (juce::Label::textColourId, bold ? kAccent : kTextDim);
        l.setJustificationType (just);
        parent.addAndMakeVisible (l);
    }

    void placeKnob (juce::Rectangle<int> cell, juce::Slider& s, juce::Label& l, int knobH = 64)
    {
        l.setBounds (cell.removeFromTop (14));
        s.setBounds (cell.removeFromTop (knobH + 16));
    }

    void placeBox (juce::Rectangle<int> cell, juce::ComboBox& b, juce::Label& l)
    {
        l.setBounds (cell.removeFromTop (14));
        b.setBounds (cell.removeFromTop (26).reduced (4, 0));
    }
}

//==============================================================================
//  Look and feel
//==============================================================================
ProLookAndFeel::ProLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId,             kBgPanelHi);
    setColour (juce::ComboBox::textColourId,                   kText);
    setColour (juce::ComboBox::outlineColourId,                kAccent.withAlpha (0.5f));
    setColour (juce::ComboBox::arrowColourId,                  kAccent);
    setColour (juce::PopupMenu::backgroundColourId,            kBgPanel);
    setColour (juce::PopupMenu::textColourId,                  kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId,       kBgDeep);
}

void ProLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle  = startAngle + pos * (endAngle - startAngle);

    // Track arc
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                         0.0f, startAngle, endAngle, true);
    g.setColour (kBgPanelHi);
    g.strokePath (track, juce::PathStrokeType (3.0f));

    // Value arc
    juce::Path val;
    val.addCentredArc (centre.x, centre.y, radius - 2.0f, radius - 2.0f,
                       0.0f, startAngle, angle, true);
    g.setColour (kAccent);
    g.strokePath (val, juce::PathStrokeType (3.0f));

    // Knob body
    const float bodyR = radius - 7.0f;
    g.setColour (kBgPanelHi);
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (kAccent.withAlpha (0.6f));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

    // Indicator
    juce::Path ind;
    const float indLen = bodyR * 0.85f;
    ind.addRoundedRectangle (-1.5f, -indLen, 3.0f, indLen, 1.5f);
    ind.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (kText);
    g.fillPath (ind);
}

void ProLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                       bool /*highlighted*/, bool /*down*/)
{
    auto bounds = b.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? kDanger : kBgPanelHi);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour ((on ? kDanger : kAccent).withAlpha (0.7f));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    g.setColour (on ? juce::Colours::white : kTextDim);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

//==============================================================================
//  ChainPill
//==============================================================================
ChainPill::ChainPill (juce::AudioProcessorValueTreeState& apvts,
                      const juce::String& paramID,
                      const juce::String& displayName,
                      bool invertActive)
    : attachment (*apvts.getParameter (paramID),
                  [this] (float v) { paramValue = v; repaint(); }),
      name (displayName),
      invert (invertActive)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

bool ChainPill::isActive() const noexcept
{
    return invert ? (paramValue > 0.5f) : (paramValue < 0.5f);
}

void ChainPill::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool active = isActive();

    const auto fill   = active ? kBgPanelHi : kBgDeep;
    const auto border = active ? kAccent    : kTextDim.withAlpha (0.3f);

    g.setColour (fill);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (border.withAlpha (hover ? 1.0f : 0.75f));
    g.drawRoundedRectangle (b, 8.0f, active ? 1.5f : 1.0f);

    g.setColour (active ? kAccent : kTextDim.withAlpha (0.55f));
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText (name, getLocalBounds(), juce::Justification::centred);

    // Small dot indicator (lit when active)
    const float dx = 8.0f, dy = 8.0f, dr = 3.0f;
    g.setColour (active ? kAccent : kTextDim.withAlpha (0.25f));
    g.fillEllipse (dx, dy, dr * 2.0f, dr * 2.0f);
}

void ChainPill::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (paramValue > 0.5f ? 0.0f : 1.0f);
}

void ChainPill::mouseEnter (const juce::MouseEvent&) { hover = true;  repaint(); }
void ChainPill::mouseExit  (const juce::MouseEvent&) { hover = false; repaint(); }

//==============================================================================
//  RoutingArrow
//==============================================================================
RoutingArrow::RoutingArrow (juce::AudioProcessorValueTreeState& apvts)
    : attachment (*apvts.getParameter ("routing"),
                  [this] (float v) { value = v; repaint(); })
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void RoutingArrow::paint (juce::Graphics& g)
{
    const bool forward = value < 0.5f; // 0 = EQ -> Comp, 1 = Comp -> EQ
    auto b = getLocalBounds().toFloat();
    const auto col = hover ? kAccent : kTextDim;

    g.setColour (col);
    juce::Path arrow;
    const float cy = b.getCentreY();
    const float pad = 6.0f;
    if (forward)
    {
        arrow.startNewSubPath (b.getX() + pad, cy);
        arrow.lineTo          (b.getRight() - pad, cy);
        arrow.lineTo          (b.getRight() - pad - 6.0f, cy - 4.0f);
        arrow.startNewSubPath (b.getRight() - pad, cy);
        arrow.lineTo          (b.getRight() - pad - 6.0f, cy + 4.0f);
    }
    else
    {
        arrow.startNewSubPath (b.getRight() - pad, cy);
        arrow.lineTo          (b.getX() + pad, cy);
        arrow.lineTo          (b.getX() + pad + 6.0f, cy - 4.0f);
        arrow.startNewSubPath (b.getX() + pad, cy);
        arrow.lineTo          (b.getX() + pad + 6.0f, cy + 4.0f);
    }
    g.strokePath (arrow, juce::PathStrokeType (1.5f));
}

void RoutingArrow::mouseDown (const juce::MouseEvent&)
{
    attachment.setValueAsCompleteGesture (value > 0.5f ? 0.0f : 1.0f);
}
void RoutingArrow::mouseEnter (const juce::MouseEvent&) { hover = true;  repaint(); }
void RoutingArrow::mouseExit  (const juce::MouseEvent&) { hover = false; repaint(); }

//==============================================================================
//  GR meter
//==============================================================================
void GainReductionMeter::timerCallback()
{
    const float target = -source.load();
    const float coef = (target > displayed) ? 0.6f : 0.15f;
    displayed += (target - displayed) * coef;
    repaint();
}

void GainReductionMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (kBgDeep);
    g.fillRoundedRectangle (b, 3.0f);

    const float frac = juce::jlimit (0.0f, 1.0f, displayed / kRange);
    auto fill = b.reduced (2.0f);
    fill = fill.removeFromLeft (fill.getWidth() * frac);
    g.setColour (kDanger);
    g.fillRoundedRectangle (fill, 2.0f);

    g.setColour (kText);
    g.setFont (10.0f);
    g.drawText (juce::String (displayed, 1) + " dB GR", getLocalBounds(),
                juce::Justification::centred);
}

//==============================================================================
//  Helper for module panels
//==============================================================================
static void paintModuleBg (juce::Component& c, juce::Graphics& g, const juce::String& titleText)
{
    auto r = c.getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (kBgPanel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (kAccent.withAlpha (0.35f));
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    g.setColour (kAccent);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText (titleText, r.toNearestInt().reduced (12, 8).removeFromTop (16),
                juce::Justification::left);
}

//==============================================================================
//  CompModule
//==============================================================================
CompModule::CompModule (ToneSuiteAudioProcessor& p)
    : proc (p), meter (p.compGainReductionDb)
{
    for (auto* s : { &input, &thresh, &makeup, &mix }) styleKnob (*s, *this);
    time.addItemList ({ "1", "2", "3", "4", "5", "6" }, 1);
    addAndMakeVisible (time);
    addAndMakeVisible (meter);

    styleLabel (title,   "COMPRESSOR", *this, 12.0f, true, juce::Justification::left);
    styleLabel (inputL,  "INPUT",      *this);
    styleLabel (threshL, "THRESH",     *this);
    styleLabel (timeL,   "TIME",       *this);
    styleLabel (makeupL, "MAKEUP",     *this);
    styleLabel (mixL,    "MIX",        *this);

    auto& v = proc.apvts;
    a1 = std::make_unique<SliderAtt> (v, "compInput",  input);
    a2 = std::make_unique<SliderAtt> (v, "compThresh", thresh);
    a3 = std::make_unique<SliderAtt> (v, "compMakeup", makeup);
    a4 = std::make_unique<SliderAtt> (v, "compMix",    mix);
    b1 = std::make_unique<BoxAtt>    (v, "compTime",   time);
}

void CompModule::paint (juce::Graphics& g) { paintModuleBg (*this, g, ""); }

void CompModule::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto header = area.removeFromTop (22);
    title.setBounds (header.removeFromLeft (140));
    meter.setBounds (header.removeFromRight (140).reduced (0, 4));

    area.removeFromTop (4);
    auto row = area.removeFromTop (110);
    const int w = row.getWidth() / 5;
    placeKnob (row.removeFromLeft (w), input,  inputL);
    placeKnob (row.removeFromLeft (w), thresh, threshL);
    placeBox  (row.removeFromLeft (w), time,   timeL);
    placeKnob (row.removeFromLeft (w), makeup, makeupL);
    placeKnob (row.removeFromLeft (w), mix,    mixL);
}

//==============================================================================
//  TapeModule
//==============================================================================
TapeModule::TapeModule (ToneSuiteAudioProcessor& p) : proc (p)
{
    for (auto* s : { &drive, &bias, &out }) styleKnob (*s, *this);
    speed.addItemList ({ "7.5 IPS", "15 IPS", "30 IPS" }, 1);
    addAndMakeVisible (speed);

    styleLabel (title,  "TAPE",   *this, 12.0f, true, juce::Justification::left);
    styleLabel (driveL, "DRIVE",  *this);
    styleLabel (biasL,  "BIAS",   *this);
    styleLabel (speedL, "SPEED",  *this);
    styleLabel (outL,   "OUTPUT", *this);

    auto& v = proc.apvts;
    a1 = std::make_unique<SliderAtt> (v, "tapeDrive", drive);
    a2 = std::make_unique<SliderAtt> (v, "tapeBias",  bias);
    a3 = std::make_unique<SliderAtt> (v, "tapeOut",   out);
    b1 = std::make_unique<BoxAtt>    (v, "tapeSpeed", speed);
}

void TapeModule::paint (juce::Graphics& g) { paintModuleBg (*this, g, ""); }

void TapeModule::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto header = area.removeFromTop (22);
    title.setBounds (header.removeFromLeft (140));

    area.removeFromTop (4);
    auto row = area.removeFromTop (110);
    const int w = row.getWidth() / 4;
    placeKnob (row.removeFromLeft (w), drive, driveL);
    placeKnob (row.removeFromLeft (w), bias,  biasL);
    placeBox  (row.removeFromLeft (w), speed, speedL);
    placeKnob (row.removeFromLeft (w), out,   outL);
}

//==============================================================================
//  DynamicsModule  -  thresh/atk/rel for each of the 3 bands
//==============================================================================
DynamicsModule::DynamicsModule (ToneSuiteAudioProcessor& p) : proc (p)
{
    styleLabel (title, "DYNAMICS", *this, 12.0f, true, juce::Justification::left);

    auto& v = proc.apvts;
    for (int i = 0; i < 3; ++i)
    {
        auto& r = rows[(size_t) i];
        const juce::String pfx = "dyn" + juce::String (i + 1);
        styleLabel (r.bandName, "BAND " + juce::String (i + 1), *this, 10.0f, true,
                    juce::Justification::centredLeft);
        r.bandName.setColour (juce::Label::textColourId,
                              i == 0 ? juce::Colour (0xffe07a5f) :
                              i == 1 ? juce::Colour (0xff81b29a) :
                                       juce::Colour (0xff8ab4f8));
        styleKnob (r.thresh, *this); styleLabel (r.threshL, "THR", *this, 9.0f);
        styleKnob (r.atk,    *this); styleLabel (r.atkL,    "ATK", *this, 9.0f);
        styleKnob (r.rel,    *this); styleLabel (r.relL,    "REL", *this, 9.0f);

        r.a1 = std::make_unique<SliderAtt> (v, pfx + "Thresh", r.thresh);
        r.a2 = std::make_unique<SliderAtt> (v, pfx + "Atk",    r.atk);
        r.a3 = std::make_unique<SliderAtt> (v, pfx + "Rel",    r.rel);
    }
}

void DynamicsModule::paint (juce::Graphics& g) { paintModuleBg (*this, g, ""); }

void DynamicsModule::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto header = area.removeFromTop (22);
    title.setBounds (header.removeFromLeft (140));
    area.removeFromTop (4);

    const int rowH = area.getHeight() / 3;
    for (auto& r : rows)
    {
        auto row = area.removeFromTop (rowH);
        r.bandName.setBounds (row.removeFromLeft (56).withTrimmedLeft (4));
        const int w = row.getWidth() / 3;
        placeKnob (row.removeFromLeft (w), r.thresh, r.threshL, 46);
        placeKnob (row.removeFromLeft (w), r.atk,    r.atkL,    46);
        placeKnob (row.removeFromLeft (w), r.rel,    r.relL,    46);
    }
}

//==============================================================================
//  ResonanceModule  -  surgical cleanup
//==============================================================================
ResonanceModule::ResonanceModule (ToneSuiteAudioProcessor& p) : proc (p)
{
    for (auto* s : { &depth, &thresh, &atk, &rel, &mix }) styleKnob (*s, *this);

    styleLabel (title,   "RESONANCE", *this, 12.0f, true, juce::Justification::left);
    styleLabel (depthL,  "DEPTH",     *this);
    styleLabel (threshL, "THRESH",    *this);
    styleLabel (atkL,    "ATTACK",    *this);
    styleLabel (relL,    "RELEASE",   *this);
    styleLabel (mixL,    "MIX",       *this);

    auto& v = proc.apvts;
    a1 = std::make_unique<SliderAtt> (v, "resDepth",  depth);
    a2 = std::make_unique<SliderAtt> (v, "resThresh", thresh);
    a3 = std::make_unique<SliderAtt> (v, "resAtk",    atk);
    a4 = std::make_unique<SliderAtt> (v, "resRel",    rel);
    a5 = std::make_unique<SliderAtt> (v, "resMix",    mix);

    startTimerHz (24);
}

ResonanceModule::~ResonanceModule() { stopTimer(); }

void ResonanceModule::timerCallback()
{
    const float target = juce::jlimit (0.0f, 1.0f, proc.resonanceActivity.load());
    const float coef = (target > activity) ? 0.5f : 0.12f;
    activity += (target - activity) * coef;
    repaint();
}

void ResonanceModule::paint (juce::Graphics& g)
{
    paintModuleBg (*this, g, "");

    // Activity indicator at the top-right corner of the panel.
    auto b = getLocalBounds().reduced (12);
    const float dr = 5.0f;
    const float x  = (float) b.getRight() - dr * 2.0f - 4.0f;
    const float y  = (float) b.getY()      + 6.0f;

    const auto base = juce::Colour (0xffd24b3e);
    if (activity > 0.04f)
    {
        const float halo = dr + 6.0f * activity;
        g.setColour (base.withAlpha (0.12f + 0.30f * activity));
        g.fillEllipse (x + dr - halo, y + dr - halo, halo * 2.0f, halo * 2.0f);
    }
    g.setColour (base.withAlpha (0.25f + 0.65f * activity));
    g.fillEllipse (x, y, dr * 2.0f, dr * 2.0f);
}

void ResonanceModule::resized()
{
    auto area = getLocalBounds().reduced (12);
    auto header = area.removeFromTop (22);
    title.setBounds (header.removeFromLeft (140));

    area.removeFromTop (4);
    auto row = area.removeFromTop (110);
    const int w = row.getWidth() / 5;
    placeKnob (row.removeFromLeft (w), depth,  depthL);
    placeKnob (row.removeFromLeft (w), thresh, threshL);
    placeKnob (row.removeFromLeft (w), atk,    atkL);
    placeKnob (row.removeFromLeft (w), rel,    relL);
    placeKnob (row.removeFromLeft (w), mix,    mixL);
}

//==============================================================================
//  Main editor
//==============================================================================
ToneSuiteAudioProcessorEditor::ToneSuiteAudioProcessorEditor (ToneSuiteAudioProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      spectrum     (p),
      dynMod       (p),
      compMod      (p),
      tapeMod      (p),
      resMod       (p),
      pillEQ       (p.apvts, "eqBypass",    "EQ"),
      pillComp     (p.apvts, "compBypass",  "COMP"),
      pillDyn      (p.apvts, "dynEqBypass", "DYN EQ"),
      pillTape     (p.apvts, "tapeOn",      "TAPE", /*invertActive=*/true),
      pillRes      (p.apvts, "resBypass",   "RESON"),
      routingArrow (p.apvts)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (dynMod);
    addAndMakeVisible (compMod);
    addAndMakeVisible (tapeMod);
    addAndMakeVisible (resMod);
    addAndMakeVisible (pillEQ);
    addAndMakeVisible (pillComp);
    addAndMakeVisible (pillDyn);
    addAndMakeVisible (pillTape);
    addAndMakeVisible (pillRes);
    addAndMakeVisible (routingArrow);

    styleKnob  (output, *this);
    styleLabel (outputL, "OUTPUT", *this);
    addAndMakeVisible (bypass);

    auto& v = proc.apvts;
    aOutput = std::make_unique<SliderAtt> (v, "output", output);
    aBypass = std::make_unique<BtnAtt>    (v, "bypass", bypass);

    setSize (1100, 860);
}

ToneSuiteAudioProcessorEditor::~ToneSuiteAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void ToneSuiteAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBgDeep);

    // Header band
    auto hdr = getLocalBounds().removeFromTop (80);
    g.setColour (kBgPanel);
    g.fillRect (hdr);
    g.setColour (kAccent.withAlpha (0.4f));
    g.drawHorizontalLine (hdr.getBottom() - 1,
                          (float) hdr.getX(), (float) hdr.getRight());

    // Title (top-left of header)
    g.setColour (kAccent);
    g.setFont (juce::Font (16.0f, juce::Font::bold));
    g.drawText ("TONE SUITE",
                getLocalBounds().reduced (20, 0).removeFromTop (80),
                juce::Justification::centredLeft);
    g.setColour (kTextDim);
    g.setFont (juce::Font (10.0f));
    g.drawText ("passive program EQ  /  Vari-Mu Comp  /  Dynamic EQ  /  Tape",
                getLocalBounds().reduced (20, 0).removeFromTop (80),
                juce::Justification::bottomLeft);
}

void ToneSuiteAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    // ---- HEADER ----
    auto hdr = r.removeFromTop (80);

    // Right side: output knob + bypass
    {
        auto right = hdr.removeFromRight (220).reduced (8, 10);
        bypass.setBounds (right.removeFromRight (90).reduced (0, 16));
        right.removeFromRight (8);
        outputL.setBounds (right.removeFromTop (14));
        output.setBounds  (right);
    }

    // Centre: chain pills with routing arrow between EQ and Comp
    {
        const int pillW = 76, pillH = 36, arrowW = 22;
        const int totalW = pillW * 5 + arrowW * 4;
        auto centre = hdr.reduced (0, (hdr.getHeight() - pillH) / 2);
        auto strip  = centre.withSizeKeepingCentre (totalW, pillH);

        pillEQ      .setBounds (strip.removeFromLeft (pillW));
        routingArrow.setBounds (strip.removeFromLeft (arrowW));
        pillComp    .setBounds (strip.removeFromLeft (pillW));
        strip.removeFromLeft (arrowW);
        pillDyn     .setBounds (strip.removeFromLeft (pillW));
        strip.removeFromLeft (arrowW);
        pillTape    .setBounds (strip.removeFromLeft (pillW));
        strip.removeFromLeft (arrowW);
        pillRes     .setBounds (strip.removeFromLeft (pillW));
    }

    // ---- SPECTRUM ----
    spectrum.setBounds (r.removeFromTop (400));

    // ---- MODULE GRID 2x2 ----
    //   [ Dynamics ][ Comp  ]
    //   [   Tape  ][ Reson  ]
    auto grid = r.reduced (8, 4);
    const int rowH = grid.getHeight() / 2;
    auto row1 = grid.removeFromTop (rowH);
    auto row2 = grid;

    const int colW = (row1.getWidth() - 8) / 2;
    dynMod .setBounds (row1.removeFromLeft (colW));
    row1.removeFromLeft (8);
    compMod.setBounds (row1.removeFromLeft (colW));

    tapeMod.setBounds (row2.removeFromLeft (colW));
    row2.removeFromLeft (8);
    resMod .setBounds (row2.removeFromLeft (colW));
}
