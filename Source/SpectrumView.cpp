#include "SpectrumView.h"

namespace
{
    const juce::Colour kBgDark     { 0xff0f1422 };
    const juce::Colour kGridDim    { 0x33ffffff };
    const juce::Colour kGridBright { 0x55ffffff };
    const juce::Colour kSpectrumFill { 0x33c9a227 };
    const juce::Colour kSpectrumLine { 0x88c9a227 };
    const juce::Colour kCurveLine    { 0xfff2e6c8 };
    const juce::Colour kCurveFill    { 0x22f2e6c8 };
    const juce::Colour kEqBoost  { 0xffc9a227 };
    const juce::Colour kEqCut    { 0xff5a8fc4 };
    const juce::Colour kDyn1         { 0xffe07a5f };
    const juce::Colour kDyn2         { 0xff81b29a };
    const juce::Colour kDyn3         { 0xff8ab4f8 };
    const juce::Colour kTextDim      { 0xaaffffff };
}

//==============================================================================
SpectrumView::SpectrumView (ToneSuiteAudioProcessor& p) : proc (p)
{
    buildHandles();
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

SpectrumView::~SpectrumView() { stopTimer(); }

//==============================================================================
void SpectrumView::setParamNormalised (const juce::String& id, float v) const
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
        p->endChangeGesture();
    }
}

void SpectrumView::setParamValue (const juce::String& id, float v) const
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->convertTo0to1 (v));
            rp->endChangeGesture();
        }
    }
}

//==============================================================================
//  Build the 7 handles. Each one is a tiny adapter between a screen position
//  and a set of APVTS params (sometimes with snapping, sometimes a sign-clamp).
//==============================================================================
void SpectrumView::buildHandles()
{
    auto& v = proc.apvts;
    auto getF = [&v] (const juce::String& id) { return v.getRawParameterValue (id)->load(); };

    // ---- helper: snap a continuous freq to a stepped choice param ----
    auto snapChoice = [this] (const juce::String& id, auto&& choices, float wantedHz)
    {
        int best = 0;
        float bestDist = std::numeric_limits<float>::max();
        const float lw = std::log (juce::jmax (wantedHz, 1.0f));
        const auto n = std::size (choices);
        for (size_t i = 0; i < n; ++i)
        {
            const float d = std::abs (lw - std::log (choices[i]));
            if (d < bestDist) { bestDist = d; best = (int) i; }
        }
        const float norm = (n > 1) ? (float) best / (float) (n - 1) : 0.0f;
        setParamNormalised (id, norm);
    };

    // ---- passive program EQ LOW BOOST ----
    {
        Handle h;
        h.name  = "Low Boost";
        h.color = kEqBoost;
        h.getFreq    = [&v, getF] { return ToneSuiteAudioProcessor::kLowFreqs[(size_t) juce::jlimit (0, 3, (int) getF ("lowFreq"))]; };
        h.getGainDb  = [getF] { return  (getF ("lowBoost") / 10.0f) * ToneSuiteAudioProcessor::kLowBoostMaxDb; };
        h.getQ       = [] { return 0.0f; };
        h.getActivity = [] { return 0.0f; };
        h.isEnabled  = [getF] { return getF ("eqBypass") < 0.5f && getF ("lowBoost") > 0.001f; };
        h.setFreq    = [this, snapChoice] (float hz)
                       { snapChoice ("lowFreq", ToneSuiteAudioProcessor::kLowFreqs, hz); };
        h.setGainDb  = [this] (float db)
                       {
                           const float dial = juce::jlimit (0.0f, 10.0f, db / ToneSuiteAudioProcessor::kLowBoostMaxDb * 10.0f);
                           setParamValue ("lowBoost", dial);
                       };
        h.setQ = [] (float) {};
        handles.push_back (std::move (h));
    }

    // ---- passive program EQ LOW ATTEN ----
    {
        Handle h;
        h.name  = "Low Atten";
        h.color = kEqCut;
        h.isCut = true;
        h.getFreq    = [&v, getF] { return ToneSuiteAudioProcessor::kLowFreqs[(size_t) juce::jlimit (0, 3, (int) getF ("lowFreq"))]; };
        h.getGainDb  = [getF] { return -(getF ("lowAtten") / 10.0f) * ToneSuiteAudioProcessor::kLowAttenMaxDb; };
        h.getQ       = [] { return 0.0f; };
        h.getActivity = [] { return 0.0f; };
        h.isEnabled  = [getF] { return getF ("eqBypass") < 0.5f && getF ("lowAtten") > 0.001f; };
        h.setFreq    = [this, snapChoice] (float hz)
                       { snapChoice ("lowFreq", ToneSuiteAudioProcessor::kLowFreqs, hz); };
        h.setGainDb  = [this] (float db)
                       {
                           const float dial = juce::jlimit (0.0f, 10.0f, (-db) / ToneSuiteAudioProcessor::kLowAttenMaxDb * 10.0f);
                           setParamValue ("lowAtten", dial);
                       };
        h.setQ = [] (float) {};
        handles.push_back (std::move (h));
    }

    // ---- passive program EQ HIGH BOOST (with bandwidth -> Q) ----
    {
        Handle h;
        h.name  = "High Boost";
        h.color = kEqBoost;
        h.getFreq    = [getF] { return ToneSuiteAudioProcessor::kHighBoostFreqs[(size_t) juce::jlimit (0, 6, (int) getF ("highFreq"))]; };
        h.getGainDb  = [getF] { return (getF ("highBoost") / 10.0f) * ToneSuiteAudioProcessor::kHighBoostMaxDb; };
        h.getQ       = [getF] { return juce::jmap (getF ("highBW"), 0.0f, 10.0f, 0.6f, 3.5f); };
        h.getActivity = [] { return 0.0f; };
        h.isEnabled  = [getF] { return getF ("eqBypass") < 0.5f && getF ("highBoost") > 0.001f; };
        h.setFreq    = [this, snapChoice] (float hz)
                       { snapChoice ("highFreq", ToneSuiteAudioProcessor::kHighBoostFreqs, hz); };
        h.setGainDb  = [this] (float db)
                       {
                           const float dial = juce::jlimit (0.0f, 10.0f, db / ToneSuiteAudioProcessor::kHighBoostMaxDb * 10.0f);
                           setParamValue ("highBoost", dial);
                       };
        h.setQ = [this] (float q)
                 {
                     const float bw = juce::jlimit (0.0f, 10.0f, juce::jmap (q, 0.6f, 3.5f, 0.0f, 10.0f));
                     setParamValue ("highBW", bw);
                 };
        handles.push_back (std::move (h));
    }

    // ---- passive program EQ HIGH ATTEN ----
    {
        Handle h;
        h.name  = "High Atten";
        h.color = kEqCut;
        h.isCut = true;
        h.getFreq    = [getF] { return ToneSuiteAudioProcessor::kHighAttenFreqs[(size_t) juce::jlimit (0, 2, (int) getF ("highAttenFreq"))]; };
        h.getGainDb  = [getF] { return -(getF ("highAtten") / 10.0f) * ToneSuiteAudioProcessor::kHighAttenMaxDb; };
        h.getQ       = [] { return 0.0f; };
        h.getActivity = [] { return 0.0f; };
        h.isEnabled  = [getF] { return getF ("eqBypass") < 0.5f && getF ("highAtten") > 0.001f; };
        h.setFreq    = [this, snapChoice] (float hz)
                       { snapChoice ("highAttenFreq", ToneSuiteAudioProcessor::kHighAttenFreqs, hz); };
        h.setGainDb  = [this] (float db)
                       {
                           const float dial = juce::jlimit (0.0f, 10.0f, (-db) / ToneSuiteAudioProcessor::kHighAttenMaxDb * 10.0f);
                           setParamValue ("highAtten", dial);
                       };
        h.setQ = [] (float) {};
        handles.push_back (std::move (h));
    }

    // ---- DYN BANDS 1..3 ----
    static const juce::Colour dynColors[3] = { kDyn1, kDyn2, kDyn3 };
    for (int i = 1; i <= 3; ++i)
    {
        const juce::String pfx = "dyn" + juce::String (i);
        Handle h;
        h.name      = "Dyn " + juce::String (i);
        h.color     = dynColors[(size_t) (i - 1)];
        h.isDynamic = true;
        const int bandIdx = i - 1;

        h.getFreq    = [&v, pfx] { return v.getRawParameterValue (pfx + "Freq")->load(); };
        h.getGainDb  = [&v, pfx] { return v.getRawParameterValue (pfx + "Gain")->load(); };
        h.getQ       = [&v, pfx] { return v.getRawParameterValue (pfx + "Q")->load(); };
        h.getActivity = [this, bandIdx] { return proc.dynBandActivity[(size_t) bandIdx].load(); };
        h.isEnabled  = [&v, pfx] { return v.getRawParameterValue ("dynEqBypass")->load() < 0.5f
                                       && v.getRawParameterValue (pfx + "On")->load() > 0.5f; };
        h.setFreq    = [this, pfx] (float hz) { setParamValue (pfx + "Freq", juce::jlimit (30.0f, 18000.0f, hz)); };
        h.setGainDb  = [this, pfx] (float db) { setParamValue (pfx + "Gain", juce::jlimit (-18.0f, 18.0f, db)); };
        h.setQ       = [this, pfx] (float q)  { setParamValue (pfx + "Q",    juce::jlimit (0.3f, 8.0f, q)); };
        handles.push_back (std::move (h));
    }
}

//==============================================================================
void SpectrumView::refreshCoefficients()
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    const double sr = juce::jmax (8000.0, proc.analyzer.getSampleRate());
    const float  nyq = (float) sr * 0.45f;

    auto get = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };

    eqBypassed  = get ("eqBypass")    > 0.5f;
    dynBypassed = get ("dynEqBypass") > 0.5f;

    // Static EQ - match the audio path exactly.
    {
        const float lowF    = ToneSuiteAudioProcessor::kLowFreqs[(size_t) juce::jlimit (0, 3, (int) get ("lowFreq"))];
        const float boostDb =  (get ("lowBoost") / 10.0f) * ToneSuiteAudioProcessor::kLowBoostMaxDb;
        const float attenDb = -(get ("lowAtten") / 10.0f) * ToneSuiteAudioProcessor::kLowAttenMaxDb;

        cLowBoost = Coeffs::makeLowShelf (sr, lowF, ToneSuiteAudioProcessor::kLowShelfQ,
                                          juce::Decibels::decibelsToGain (boostDb));
        cLowCut   = Coeffs::makeLowShelf (sr,
                                          juce::jmin (lowF * ToneSuiteAudioProcessor::kLowCutRatio, nyq),
                                          ToneSuiteAudioProcessor::kLowShelfQ,
                                          juce::Decibels::decibelsToGain (attenDb));

        const float highF    = ToneSuiteAudioProcessor::kHighBoostFreqs[(size_t) juce::jlimit (0, 6, (int) get ("highFreq"))];
        const float hBoostDb = (get ("highBoost") / 10.0f) * ToneSuiteAudioProcessor::kHighBoostMaxDb;
        const float q        = juce::jmap (get ("highBW"), 0.0f, 10.0f, 0.6f, 3.5f);
        cHighBoost = Coeffs::makePeakFilter (sr, juce::jmin (highF, nyq), q,
                                             juce::Decibels::decibelsToGain (hBoostDb));

        const float haF       = ToneSuiteAudioProcessor::kHighAttenFreqs[(size_t) juce::jlimit (0, 2, (int) get ("highAttenFreq"))];
        const float hAttenDb  = -(get ("highAtten") / 10.0f) * ToneSuiteAudioProcessor::kHighAttenMaxDb;
        cHighCut = Coeffs::makeHighShelf (sr, juce::jmin (haF, nyq), 0.7f,
                                          juce::Decibels::decibelsToGain (hAttenDb));
    }

    // Dynamic bands: show the band's CURRENT effective gain (target * activity)
    // so the curve animates as the band reacts to audio.
    for (int i = 0; i < 3; ++i)
    {
        const juce::String pfx = "dyn" + juce::String (i + 1);
        const bool on = get ((pfx + "On").toRawUTF8()) > 0.5f;
        if (! on)
        {
            cDynPeak[(size_t) i].reset();
            continue;
        }
        const float freq = get ((pfx + "Freq").toRawUTF8());
        const float q    = get ((pfx + "Q").toRawUTF8());
        const float tgt  = get ((pfx + "Gain").toRawUTF8());
        const float act  = proc.dynBandActivity[(size_t) i].load();
        const float effDb = tgt * act;
        cDynPeak[(size_t) i] = Coeffs::makePeakFilter (sr, juce::jmin (freq, nyq), q,
                                                       juce::Decibels::decibelsToGain (effDb));
    }
}

float SpectrumView::computeEQDbAt (float freq) const
{
    const double sr = juce::jmax (8000.0, proc.analyzer.getSampleRate());
    float total = 0.0f;

    if (! eqBypassed)
    {
        if (cLowBoost  != nullptr) total += juce::Decibels::gainToDecibels (cLowBoost ->getMagnitudeForFrequency (freq, sr));
        if (cLowCut    != nullptr) total += juce::Decibels::gainToDecibels (cLowCut   ->getMagnitudeForFrequency (freq, sr));
        if (cHighBoost != nullptr) total += juce::Decibels::gainToDecibels (cHighBoost->getMagnitudeForFrequency (freq, sr));
        if (cHighCut   != nullptr) total += juce::Decibels::gainToDecibels (cHighCut  ->getMagnitudeForFrequency (freq, sr));
    }
    if (! dynBypassed)
    {
        for (auto& c : cDynPeak)
            if (c != nullptr)
                total += juce::Decibels::gainToDecibels (c->getMagnitudeForFrequency (freq, sr));
    }
    return total;
}

//==============================================================================
void SpectrumView::timerCallback()
{
    proc.analyzer.computeMagnitudes();
    refreshCoefficients();
    repaint();
}

void SpectrumView::resized() {}

juce::Rectangle<int> SpectrumView::plotArea() const
{
    return getLocalBounds().reduced (12);
}

float SpectrumView::freqToX (float freq, juce::Rectangle<int> p) const
{
    const float t = std::log (freq / kFreqMin) / std::log (kFreqMax / kFreqMin);
    return (float) p.getX() + t * (float) p.getWidth();
}
float SpectrumView::xToFreq (float x, juce::Rectangle<int> p) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - (float) p.getX()) / (float) p.getWidth());
    return kFreqMin * std::pow (kFreqMax / kFreqMin, t);
}
float SpectrumView::dbToY (float db, juce::Rectangle<int> p) const
{
    const float t = (db - kDbMin) / (kDbMax - kDbMin);
    return (float) p.getBottom() - t * (float) p.getHeight();
}
float SpectrumView::yToDb (float y, juce::Rectangle<int> p) const
{
    const float t = juce::jlimit (0.0f, 1.0f, ((float) p.getBottom() - y) / (float) p.getHeight());
    return kDbMin + t * (kDbMax - kDbMin);
}
float SpectrumView::spectrumDbToY (float db, juce::Rectangle<int> p) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (db - kSpectrumDbMin) / (kSpectrumDbMax - kSpectrumDbMin));
    return (float) p.getBottom() - t * (float) p.getHeight();
}

//==============================================================================
void SpectrumView::drawGrid (juce::Graphics& g, juce::Rectangle<int> plot)
{
    // Frequency grid lines
    g.setColour (kGridDim);
    static const float majorF[] = { 100.0f, 1000.0f, 10000.0f };
    static const float minorF[] = { 30.0f, 50.0f, 70.0f, 200.0f, 300.0f, 500.0f, 700.0f,
                                    2000.0f, 3000.0f, 5000.0f, 7000.0f, 20000.0f };

    for (float f : minorF)
    {
        const float x = freqToX (f, plot);
        g.drawVerticalLine ((int) x, (float) plot.getY(), (float) plot.getBottom());
    }

    g.setColour (kGridBright);
    for (float f : majorF)
    {
        const float x = freqToX (f, plot);
        g.drawVerticalLine ((int) x, (float) plot.getY(), (float) plot.getBottom());
    }

    // dB grid lines
    g.setColour (kGridDim);
    for (int db = -24; db <= 24; db += 6)
        if (db != 0)
            g.drawHorizontalLine ((int) dbToY ((float) db, plot), (float) plot.getX(), (float) plot.getRight());

    g.setColour (kGridBright);
    g.drawHorizontalLine ((int) dbToY (0.0f, plot), (float) plot.getX(), (float) plot.getRight());

    // Labels
    g.setColour (kTextDim);
    g.setFont (10.0f);
    for (float f : majorF)
    {
        const float x = freqToX (f, plot);
        juce::String s = (f >= 1000.0f) ? (juce::String (f / 1000.0f, 0) + "k") : juce::String ((int) f);
        g.drawText (s, (int) x + 2, plot.getBottom() - 14, 40, 12, juce::Justification::left);
    }
    for (int db : { -18, -12, -6, 6, 12, 18 })
        g.drawText (juce::String (db),
                    plot.getX() + 2, (int) dbToY ((float) db, plot) - 6, 30, 12,
                    juce::Justification::left);
}

void SpectrumView::drawSpectrum (juce::Graphics& g, juce::Rectangle<int> plot)
{
    juce::Path path;
    const int steps = juce::jmax (16, plot.getWidth());
    bool started = false;

    for (int i = 0; i <= steps; ++i)
    {
        const float x = (float) plot.getX() + (float) i * (float) plot.getWidth() / (float) steps;
        const float f = xToFreq (x, plot);
        const float db = proc.analyzer.getMagnitudeAtFreq (f);
        const float y = juce::jlimit ((float) plot.getY(), (float) plot.getBottom(), spectrumDbToY (db, plot));

        if (! started) { path.startNewSubPath (x, y); started = true; }
        else            path.lineTo (x, y);
    }

    // Filled area underneath
    juce::Path fill = path;
    fill.lineTo ((float) plot.getRight(), (float) plot.getBottom());
    fill.lineTo ((float) plot.getX(),     (float) plot.getBottom());
    fill.closeSubPath();

    g.setColour (kSpectrumFill);
    g.fillPath (fill);
    g.setColour (kSpectrumLine);
    g.strokePath (path, juce::PathStrokeType (1.2f));
}

void SpectrumView::drawEQCurve (juce::Graphics& g, juce::Rectangle<int> plot)
{
    juce::Path path;
    const int steps = juce::jmax (32, plot.getWidth() / 2);
    bool started = false;

    for (int i = 0; i <= steps; ++i)
    {
        const float x  = (float) plot.getX() + (float) i * (float) plot.getWidth() / (float) steps;
        const float f  = xToFreq (x, plot);
        const float db = computeEQDbAt (f);
        const float y  = juce::jlimit ((float) plot.getY(), (float) plot.getBottom(), dbToY (db, plot));

        if (! started) { path.startNewSubPath (x, y); started = true; }
        else            path.lineTo (x, y);
    }

    // Fill between curve and 0 dB line
    juce::Path fill = path;
    const float zeroY = dbToY (0.0f, plot);
    fill.lineTo ((float) plot.getRight(), zeroY);
    fill.lineTo ((float) plot.getX(),     zeroY);
    fill.closeSubPath();

    g.setColour (kCurveFill);
    g.fillPath (fill);
    g.setColour (kCurveLine);
    g.strokePath (path, juce::PathStrokeType (2.2f));
}

void SpectrumView::drawHandles (juce::Graphics& g, juce::Rectangle<int> plot)
{
    for (size_t i = 0; i < handles.size(); ++i)
    {
        const auto& h = handles[i];
        const bool enabled = h.isEnabled();
        const float freq   = h.getFreq();
        const float gainDb = h.getGainDb();
        const float x = freqToX (freq, plot);
        const float y = dbToY (juce::jlimit (kDbMin, kDbMax, gainDb), plot);

        const auto col = enabled ? h.color : h.color.withAlpha (0.35f);

        // Activity halo for dynamic bands
        if (h.isDynamic && enabled)
        {
            const float act = juce::jlimit (0.0f, 1.0f, h.getActivity());
            if (act > 0.01f)
            {
                const float r = kHandleRadius + 6.0f + act * 18.0f;
                g.setColour (h.color.withAlpha (0.18f + 0.32f * act));
                g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
            }
        }

        // Connecting stem to the 0-dB line, for clarity
        g.setColour (col.withAlpha (0.4f));
        g.drawLine (x, y, x, dbToY (0.0f, plot), 1.0f);

        // The handle itself
        const float r = kHandleRadius + ((int) i == hoveredIdx ? 2.0f : 0.0f);
        g.setColour (col);
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (enabled ? 0.85f : 0.4f));
        g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 1.5f);

        // Label on hover or drag
        if ((int) i == hoveredIdx || (int) i == draggingIdx)
        {
            const juce::String txt = h.name + "  "
                                   + juce::String (gainDb, 1) + " dB  "
                                   + (freq >= 1000.0f
                                      ? juce::String (freq / 1000.0f, 1) + " kHz"
                                      : juce::String ((int) freq) + " Hz");
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            const int tw = 200, th = 16;
            const int tx = juce::jlimit (plot.getX(), plot.getRight() - tw, (int) x - tw / 2);
            const int ty = juce::jlimit (plot.getY(), plot.getBottom() - th - 4, (int) y - 28);
            g.fillRoundedRectangle ((float) tx, (float) ty, (float) tw, (float) th, 4.0f);
            g.setColour (juce::Colours::white);
            g.setFont (11.0f);
            g.drawText (txt, tx, ty, tw, th, juce::Justification::centred);
        }
    }
}

//==============================================================================
void SpectrumView::paint (juce::Graphics& g)
{
    g.fillAll (kBgDark);
    auto plot = plotArea();
    drawGrid     (g, plot);
    drawSpectrum (g, plot);
    drawEQCurve  (g, plot);
    drawHandles  (g, plot);

    // border
    g.setColour (kGridBright);
    g.drawRect (plot, 1);
}

//==============================================================================
int SpectrumView::findHandleAt (juce::Point<float> p, juce::Rectangle<int> plot) const
{
    int best = -1;
    float bestDist = (kHandleRadius + 4.0f) * (kHandleRadius + 4.0f);
    for (size_t i = 0; i < handles.size(); ++i)
    {
        const float x = freqToX (handles[i].getFreq(), plot);
        const float y = dbToY (juce::jlimit (kDbMin, kDbMax, handles[i].getGainDb()), plot);
        const float dx = p.x - x, dy = p.y - y;
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDist) { bestDist = d2; best = (int) i; }
    }
    return best;
}

void SpectrumView::mouseMove (const juce::MouseEvent& e)
{
    const int newH = findHandleAt (e.position, plotArea());
    if (newH != hoveredIdx) { hoveredIdx = newH; repaint(); }
}

void SpectrumView::mouseExit (const juce::MouseEvent&)
{
    if (hoveredIdx != -1) { hoveredIdx = -1; repaint(); }
}

void SpectrumView::mouseDown (const juce::MouseEvent& e)
{
    draggingIdx = findHandleAt (e.position, plotArea());
}

void SpectrumView::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingIdx < 0) return;
    auto plot = plotArea();
    auto& h = handles[(size_t) draggingIdx];
    const float f  = xToFreq (e.position.x, plot);
    const float db = yToDb   (e.position.y, plot);
    h.setFreq   (f);
    h.setGainDb (db);
    repaint();
}

void SpectrumView::mouseUp (const juce::MouseEvent&) { draggingIdx = -1; }

void SpectrumView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int idx = (draggingIdx >= 0) ? draggingIdx : findHandleAt (e.position, plotArea());
    if (idx < 0) return;
    auto& h = handles[(size_t) idx];
    const float curQ = h.getQ();
    if (curQ <= 0.0f) return; // handle has no Q

    const float newQ = juce::jlimit (0.3f, 8.0f, curQ * (1.0f + wheel.deltaY * 0.5f));
    h.setQ (newQ);
    repaint();
}
