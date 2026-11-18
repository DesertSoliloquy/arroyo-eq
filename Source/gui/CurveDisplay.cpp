#include "CurveDisplay.h"
#include "Theme.h"

using namespace arroyo;

namespace
{
constexpr double kMinF = 20.0, kMaxF = 20000.0;
constexpr double kSpecTop = 6.0, kSpecBottom = -84.0;

juce::String freqLabel (double f)
{
    if (f >= 1000.0)
        return juce::String (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz";
    return juce::String (f, f < 100.0 ? 1 : 0) + " Hz";
}
} // namespace

CurveDisplay::CurveDisplay (ArroyoProcessor& p, std::function<void (int)> cb)
    : proc (p), onSelect (std::move (cb))
{
    fftData.assign ((size_t) fftSize * 2, 0.0f);
    pullBuf.assign (1 << 15, 0.0f);
    for (auto* s : { &pre, &post })
    {
        s->history.assign ((size_t) fftSize, 0.0f);
        s->db.assign ((size_t) fftSize / 2, (float) kSpecBottom - 20.0f);
    }
    // amplitude normalisation so a full-scale sine reads 0 dB
    std::vector<float> w ((size_t) fftSize, 1.0f);
    window.multiplyWithWindowingTable (w.data(), (size_t) fftSize);
    float sum = 0;
    for (auto v : w) sum += v;
    windowGain = 2.0f / sum;
    setOpaque (true);
}

juce::Rectangle<float> CurveDisplay::plot() const
{
    return getLocalBounds().toFloat().withTrimmedBottom (18.0f).withTrimmedRight (34.0f).reduced (1.0f);
}

float CurveDisplay::freqToX (double f) const
{
    const auto r = plot();
    return r.getX() + (float) (std::log (f / kMinF) / std::log (kMaxF / kMinF)) * r.getWidth();
}

double CurveDisplay::xToFreq (float x) const
{
    const auto r = plot();
    return kMinF * std::pow (kMaxF / kMinF, (double) ((x - r.getX()) / r.getWidth()));
}

double CurveDisplay::eqRange() const
{
    const int idx = (int) proc.apvts.state.getProperty ("range", 1);
    return idx == 0 ? 12.0 : (idx == 2 ? 36.0 : 24.0);
}

float CurveDisplay::dbToY (double db) const
{
    const auto r = plot();
    const double range = eqRange();
    return r.getCentreY() - (float) (db / range) * (r.getHeight() * 0.5f - 6.0f);
}

double CurveDisplay::yToDb (float y) const
{
    const auto r = plot();
    return (r.getCentreY() - y) / (r.getHeight() * 0.5f - 6.0f) * eqRange();
}

float CurveDisplay::specDbToY (double db) const
{
    const auto r = plot();
    return r.getY() + (float) ((kSpecTop - db) / (kSpecTop - kSpecBottom)) * r.getHeight();
}

juce::RangedAudioParameter* CurveDisplay::param (int b, const char* what) const
{
    return proc.apvts.getParameter (ids::band (b, what));
}

void CurveDisplay::setParam (int b, const char* what, float value)
{
    if (auto* p = param (b, what))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void CurveDisplay::gesture (int b, bool begin)
{
    for (auto* w : { "freq", "gain", "q" })
        if (auto* p = param (b, w))
            begin ? p->beginChangeGesture() : p->endChangeGesture();
}

juce::Point<float> CurveDisplay::nodePos (int, const BandSpec& s) const
{
    const float y = typeHasGain (s.type) ? dbToY (s.gainDb) : dbToY (0.0);
    return { freqToX (s.freq), y };
}

int CurveDisplay::hitNode (juce::Point<float> pt) const
{
    int best = -1;
    float bestD = 11.0f;
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto s = proc.getBandSpec (b);
        if (! s.on && b != selected)
            continue;
        const float d = nodePos (b, s).getDistanceFrom (pt);
        if (d < bestD)
        {
            bestD = d;
            best = b;
        }
    }
    return best;
}

// ---------------------------------------------------------------- analyzer
void CurveDisplay::processTap (AnalyzerTap& tap, Spectrum& s, bool wanted)
{
    int got = tap.pull (pullBuf.data(), (int) pullBuf.size());
    s.active = wanted;
    if (! wanted)
        return;

    if (got > 0)
    {
        auto& h = s.history;
        if (got >= fftSize)
            std::copy (pullBuf.begin() + (got - fftSize), pullBuf.begin() + got, h.begin());
        else
        {
            std::move (h.begin() + got, h.end(), h.begin());
            std::copy (pullBuf.begin(), pullBuf.begin() + got, h.end() - got);
        }
    }

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::copy (s.history.begin(), s.history.end(), fftData.begin());
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

    const double fs = proc.getCurrentSampleRate();
    const int tiltIdx = (int) proc.apvts.state.getProperty ("tilt", 2);
    const double tilt = tiltIdx == 0 ? 0.0 : (tiltIdx == 1 ? 3.0 : (tiltIdx == 2 ? 4.5 : 6.0));
    const float decay = 0.75f; // dB per frame (~22 dB/s)
    for (int i = 1; i < fftSize / 2; ++i)
    {
        const double f = i * fs / fftSize;
        float raw = juce::Decibels::gainToDecibels (fftData[(size_t) i] * windowGain, -140.0f);
        raw += (float) (tilt * std::log2 (std::max (f, 1.0) / 1000.0));
        float& d = s.db[(size_t) i];
        d = raw > d ? d + 0.6f * (raw - d) : std::max (raw, d - decay);
    }
}

void CurveDisplay::updateAnalyzer()
{
    const int mode = (int) proc.apvts.state.getProperty ("analyzer", 2);
    processTap (proc.preTap, pre, mode == 1 || mode == 3);
    processTap (proc.postTap, post, mode == 2 || mode == 3);
    repaint();
}

void CurveDisplay::drawSpectrum (juce::Graphics& g, const Spectrum& s, juce::Colour c) const
{
    if (! s.active)
        return;
    const auto r = plot();
    const double fs = proc.getCurrentSampleRate();
    const double binHz = fs / fftSize;
    juce::Path p;
    bool started = false;
    const float step = 2.0f;
    for (float x = r.getX(); x <= r.getRight(); x += step)
    {
        const double f0 = xToFreq (x), f1 = xToFreq (x + step);
        const double b0 = f0 / binHz, b1 = f1 / binHz;
        float db;
        if (b1 - b0 < 1.0)
        {
            // fewer than one bin per column: interpolate
            const double b = 0.5 * (b0 + b1);
            const int i = juce::jlimit (1, fftSize / 2 - 2, (int) b);
            const float t = (float) (b - i);
            db = s.db[(size_t) i] + t * (s.db[(size_t) i + 1] - s.db[(size_t) i]);
        }
        else
        {
            db = -200.0f;
            for (int i = juce::jmax (1, (int) b0); i <= juce::jmin (fftSize / 2 - 1, (int) b1); ++i)
                db = std::max (db, s.db[(size_t) i]);
        }
        const float y = juce::jlimit (r.getY(), r.getBottom(), specDbToY (db));
        if (! started) { p.startNewSubPath (x, r.getBottom()); p.lineTo (x, y); started = true; }
        else p.lineTo (x, y);
    }
    if (! started)
        return;
    p.lineTo (r.getRight(), r.getBottom());
    p.closeSubPath();
    g.setGradientFill (juce::ColourGradient (c.withAlpha (0.38f), 0, r.getY(), c.withAlpha (0.06f), 0, r.getBottom(), false));
    g.fillPath (p);
    g.setColour (c.withAlpha (0.55f));
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

// ---------------------------------------------------------------- painting
void CurveDisplay::drawGrid (juce::Graphics& g) const
{
    const auto r = plot();
    g.setFont (theme::font (11.0f));
    for (double f : { 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0,
                      800.0, 900.0, 1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0, 10000.0, 20000.0 })
    {
        const float x = freqToX (f);
        const bool major = juce::approximatelyEqual (f, 100.0) || juce::approximatelyEqual (f, 1000.0) || juce::approximatelyEqual (f, 10000.0);
        g.setColour (theme::grid.withAlpha (major ? 1.0f : 0.55f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    }
    g.setColour (theme::dim);
    for (double f : { 30.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = freqToX (f);
        const juce::String t = f >= 1000.0 ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f);
        g.drawText (t, juce::Rectangle<float> (x - 20, r.getBottom() + 2, 40, 14), juce::Justification::centred);
    }
    const double range = eqRange();
    const double stepDb = range <= 12.0 ? 3.0 : (range <= 24.0 ? 6.0 : 12.0);
    for (double d = -range; d <= range + 0.01; d += stepDb)
    {
        const float y = dbToY (d);
        g.setColour (std::abs (d) < 0.01 ? theme::grid.brighter (0.35f) : theme::grid.withAlpha (0.7f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
        g.setColour (theme::dim);
        g.drawText ((d > 0 ? "+" : "") + juce::String ((int) d), juce::Rectangle<float> (r.getRight() + 2, y - 7, 30, 14),
                    juce::Justification::centredLeft);
    }
}

void CurveDisplay::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);
    const auto r = plot();
    g.setColour (theme::panel);
    g.fillRect (r);
    drawGrid (g);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        drawSpectrum (g, pre, theme::dim);
        drawSpectrum (g, post, theme::accent2);
    }

    const double fs = proc.getCurrentSampleRate();
    const bool eqOn = proc.apvts.getRawParameterValue ("eq_on")->load() > 0.5f;
    BandSpec specs[kNumBands];
    for (int b = 0; b < kNumBands; ++b)
        specs[b] = proc.getBandSpec (b);

    // per-band curves and composite
    const int nPts = juce::jmax (2, (int) r.getWidth() / 2);
    std::vector<float> xs ((size_t) nPts), total ((size_t) nPts, 0.0f);
    std::vector<std::vector<float>> per (kNumBands);
    for (int i = 0; i < nPts; ++i)
        xs[(size_t) i] = r.getX() + r.getWidth() * i / (nPts - 1);
    for (int b = 0; b < kNumBands; ++b)
    {
        if (! specs[b].on)
            continue;
        per[(size_t) b].resize ((size_t) nPts);
        for (int i = 0; i < nPts; ++i)
        {
            const float db = (float) (20.0 * std::log10 (std::abs (bandResponse (specs[b], fs, xToFreq (xs[(size_t) i]))) + 1e-12));
            per[(size_t) b][(size_t) i] = db;
            total[(size_t) i] += db;
        }
    }

    juce::Graphics::ScopedSaveState clip (g);
    g.reduceClipRegion (r.toNearestInt());
    const float zeroY = dbToY (0.0);
    for (int b = 0; b < kNumBands; ++b)
    {
        if (per[(size_t) b].empty())
            continue;
        juce::Path p;
        p.startNewSubPath (xs[0], zeroY);
        for (int i = 0; i < nPts; ++i)
            p.lineTo (xs[(size_t) i], dbToY (juce::jmax (-200.0f, per[(size_t) b][(size_t) i])));
        p.lineTo (xs.back(), zeroY);
        p.closeSubPath();
        const auto c = theme::band (b);
        const bool sel = b == selected || b == hovered;
        g.setColour (c.withAlpha (sel ? 0.22f : 0.07f));
        g.fillPath (p);
        g.setColour (c.withAlpha (sel ? 0.8f : 0.35f));
        g.strokePath (p, juce::PathStrokeType (sel ? 1.4f : 1.0f));
    }

    juce::Path curve;
    for (int i = 0; i < nPts; ++i)
    {
        const float y = dbToY (juce::jmax (-200.0f, total[(size_t) i]));
        i == 0 ? curve.startNewSubPath (xs[0], y) : curve.lineTo (xs[(size_t) i], y);
    }
    g.setColour (theme::accent.withAlpha (eqOn ? 0.35f : 0.12f));
    g.strokePath (curve, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (eqOn ? theme::curve : theme::dim);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // nodes
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& s = specs[b];
        if (! s.on && b != selected)
            continue;
        const auto pos = nodePos (b, s);
        const auto c = theme::band (b);
        const bool sel = b == selected, hov = b == hovered;
        const float rad = sel ? 9.0f : 7.5f;
        auto circle = juce::Rectangle<float> (rad * 2, rad * 2).withCentre (pos);
        if (s.on)
        {
            g.setColour (c.withAlpha (0.9f));
            g.fillEllipse (circle);
        }
        else
        {
            g.setColour (theme::bg.withAlpha (0.8f));
            g.fillEllipse (circle);
            g.setColour (c.withAlpha (0.8f));
            g.drawEllipse (circle, 1.5f);
        }
        if (sel || hov)
        {
            g.setColour (juce::Colours::white.withAlpha (sel ? 0.95f : 0.6f));
            g.drawEllipse (circle.expanded (2.5f), 1.5f);
        }
        g.setColour (s.on ? theme::bg : c);
        g.setFont (theme::font (10.5f, true));
        g.drawText (juce::String (b + 1), circle, juce::Justification::centred);
    }

    // readout for hovered/selected band
    const int shown = dragging >= 0 ? dragging : (hovered >= 0 ? hovered : selected);
    if (shown >= 0)
    {
        const auto& s = specs[shown];
        juce::String t = "Band " + juce::String (shown + 1) + "  " + bandTypeNames()[(int) s.type] + "  " + freqLabel (s.freq);
        if (typeHasGain (s.type))
            t << "  " << (s.gainDb > 0 ? "+" : "") << juce::String (s.gainDb, 1) << " dB";
        if (typeHasSlope (s.type))
            t << "  " << slopeNames()[s.slopeIndex];
        t << "  Q " << juce::String (s.q, 2);
        if (! s.on)
            t << "  (off)";
        g.setFont (theme::font (12.5f, true));
        const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 20.0f;
        auto box = juce::Rectangle<float> (r.getX() + 8, r.getY() + 8, w, 22);
        g.setColour (theme::bg.withAlpha (0.82f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (theme::band (shown));
        g.drawText (t, box, juce::Justification::centred);
    }

    if (! eqOn)
    {
        g.setFont (theme::font (14.0f, true));
        g.setColour (theme::dim);
        g.drawText ("EQ BYPASSED", r.withTrimmedTop (8).withHeight (22).withTrimmedRight (10), juce::Justification::centredRight);
    }
}

// ---------------------------------------------------------------- mouse
void CurveDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hovered)
    {
        hovered = h;
        repaint();
    }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void CurveDisplay::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void CurveDisplay::mouseDown (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h < 0)
        return;
    selected = h;
    if (onSelect) onSelect (h);
    if (e.mods.isPopupMenu())
    {
        showBandMenu (h);
        return;
    }
    const auto s = proc.getBandSpec (h);
    dragging = h;
    dragStart = e.position;
    dragStartFreq = (float) s.freq;
    dragStartGain = (float) s.gainDb;
    dragStartQ = (float) s.q;
    gesture (h, true);
    repaint();
}

void CurveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;
    const auto s = proc.getBandSpec (dragging);
    const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
    const auto r = plot();
    const float startX = freqToX (dragStartFreq);
    const float x = juce::jlimit (r.getX(), r.getRight(), startX + (e.position.x - dragStart.x) * fine);
    setParam (dragging, "freq", (float) juce::jlimit (10.0, 22000.0, xToFreq (x)));
    const float dy = (e.position.y - dragStart.y) * fine;
    if (typeHasGain (s.type))
    {
        const float startY = dbToY (dragStartGain);
        setParam (dragging, "gain", (float) juce::jlimit (-24.0, 24.0, yToDb (startY + dy)));
    }
    else
        setParam (dragging, "q", juce::jlimit (0.1f, 18.0f, dragStartQ * std::pow (2.0f, -dy / 40.0f)));
    repaint();
}

void CurveDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        gesture (dragging, false);
    dragging = -1;
    repaint();
}

void CurveDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h >= 0)
    {
        // double-click a node: switch the band off
        setParam (h, "on", proc.getBandSpec (h).on ? 0.0f : 1.0f);
        return;
    }
    // double-click empty space: bring in the first unused band here
    for (int b = 0; b < kNumBands; ++b)
    {
        if (proc.getBandSpec (b).on)
            continue;
        const double f = juce::jlimit (10.0, 22000.0, xToFreq (e.position.x));
        setParam (b, "type", (float) BandType::Bell);
        setParam (b, "freq", (float) f);
        setParam (b, "gain", (float) juce::jlimit (-24.0, 24.0, yToDb (e.position.y)));
        setParam (b, "q", 1.0f);
        setParam (b, "on", 1.0f);
        selected = b;
        if (onSelect) onSelect (b);
        repaint();
        return;
    }
}

void CurveDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = hitNode (e.position);
    if (b < 0)
        b = selected;
    if (b < 0)
        return;
    const auto s = proc.getBandSpec (b);
    auto* p = param (b, "q");
    p->beginChangeGesture();
    setParam (b, "q", juce::jlimit (0.1f, 18.0f, (float) s.q * std::pow (1.25f, w.deltaY * 4.0f)));
    p->endChangeGesture();
    repaint();
}

void CurveDisplay::showBandMenu (int b)
{
    const auto s = proc.getBandSpec (b);
    juce::PopupMenu m, types, slopes;
    for (int t = 0; t < kNumBandTypes; ++t)
        types.addItem (100 + t, bandTypeNames()[t], true, (int) s.type == t);
    for (int i = 0; i < 4; ++i)
        slopes.addItem (200 + i, slopeNames()[i], typeHasSlope (s.type), s.slopeIndex == i);
    m.addSectionHeader ("Band " + juce::String (b + 1));
    m.addSubMenu ("Type", types);
    m.addSubMenu ("Slope", slopes, typeHasSlope (s.type));
    m.addItem (1, s.on ? "Turn band off" : "Turn band on");
    m.addItem (2, "Reset gain and Q");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [this, b] (int r)
                     {
                         if (r >= 100 && r < 100 + kNumBandTypes) setParam (b, "type", (float) (r - 100));
                         else if (r >= 200 && r < 204) setParam (b, "slope", (float) (r - 200));
                         else if (r == 1) setParam (b, "on", proc.getBandSpec (b).on ? 0.0f : 1.0f);
                         else if (r == 2) { setParam (b, "gain", 0.0f); setParam (b, "q", 0.71f); }
                         repaint();
                     });
}
