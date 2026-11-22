#include "CompEqPage.h"
#include <cmath>

using namespace ui;

namespace
{
    juce::String dbText (int db) { return (db > 0 ? "+" : "") + juce::String (db) + " dB"; }

    juce::String hzText (double hz)
    {
        if (hz < 1000.0)
            return juce::String ((int) std::lround (hz)) + " Hz";
        juce::String s (hz / 1000.0, 1);
        s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s + " kHz";
    }

    int choiceHz (const mc909::ParamDef& d, int raw)
    {
        return d.choices[juce::jlimit (0, d.choices.size() - 1, raw)].getIntValue();
    }

    /** Index of the choice whose frequency is closest (on a log scale) to `hz`. */
    int nearestChoice (const mc909::ParamDef& d, double hz)
    {
        int best = 0;
        double bestDist = 1.0e9;
        for (int i = 0; i < d.choices.size(); ++i)
        {
            const double dist = std::abs (std::log (hz / (double) d.choices[i].getIntValue()));
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
        return best;
    }
}

//==============================================================================
CompSchematic::CompSchematic (MC909EditorProcessor& p)
    : ParamControl (p), threshold (findParam ("comp.threshold")), ratio (findParam ("comp.ratio"))
{
}

double CompSchematic::ratioFor (int step)
{
    return step >= 19 ? 1.0e9 : 1.0 / (1.0 - (double) juce::jmax (0, step) / 19.0);
}

juce::Rectangle<float> CompSchematic::plot() const
{
    auto b = getLocalBounds().toFloat();
    return { b.getX() + 12.0f, b.getY() + 12.0f, b.getWidth() - 24.0f, b.getHeight() - 30.0f };
}

float CompSchematic::xFor (double v) const { const auto p = plot(); return p.getX() + (float) v * p.getWidth(); }
float CompSchematic::yFor (double v) const { const auto p = plot(); return p.getBottom() - (float) v * p.getHeight(); }
double CompSchematic::vForX (float x) const { const auto p = plot(); return (double) ((x - p.getX()) / p.getWidth()); }
double CompSchematic::vForY (float y) const { const auto p = plot(); return (double) ((p.getBottom() - y) / p.getHeight()); }

double CompSchematic::kneeV() const
{
    return juce::jlimit (0.0, 1.0, (double) proc.getParameterValue (threshold) / 127.0);
}

double CompSchematic::endV() const
{
    const double t = kneeV();
    return t + (1.0 - t) / ratioFor (proc.getParameterValue (ratio));
}

void CompSchematic::mouseDown (const juce::MouseEvent& e)
{
    const float dk = e.position.getDistanceFrom ({ xFor (kneeV()), yFor (kneeV()) });
    const float de = e.position.getDistanceFrom ({ xFor (1.0), yFor (endV()) });
    drag = (kneeV() < 0.97 && de < dk && de < 16.0f) ? end : knee;
    mouseDrag (e);
}

void CompSchematic::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == knee)
    {
        const int raw = juce::jlimit (0, 127, (int) std::lround (vForX (e.position.x) * 127.0));
        if (raw != proc.getParameterValue (threshold))
            proc.setParameterValue (threshold, raw);
    }
    else if (drag == end)
    {
        const double t = kneeV();
        const double want = juce::jlimit (t, 1.0, vForY (e.position.y));

        int best = 0;
        double bestDist = 1.0e9;
        for (int i = 0; i < 20; ++i)
        {
            const double d = std::abs (t + (1.0 - t) / ratioFor (i) - want);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (best != proc.getParameterValue (ratio))
            proc.setParameterValue (ratio, best);
    }
}

void CompSchematic::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = findColour (accentColourId, true);
    const auto pl = plot();

    g.setColour (col::well);
    g.fillRoundedRectangle (b, 6.0f);

    for (int i = 1; i < 4; ++i)
    {
        g.setColour (col::cardEdge.withAlpha (0.55f));
        g.fillRect (xFor (i / 4.0), pl.getY(), 1.0f, pl.getHeight());
        g.fillRect (pl.getX(), yFor (i / 4.0), pl.getWidth(), 1.0f);
    }
    g.setColour (col::cardEdge);
    g.drawRect (pl, 1.0f);

    juce::Path diag, dashed;
    diag.startNewSubPath (xFor (0.0), yFor (0.0));
    diag.lineTo (xFor (1.0), yFor (1.0));
    const float dashes[2] { 4.0f, 4.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, diag, dashes, 2);
    g.setColour (col::textDim.withAlpha (0.55f));
    g.fillPath (dashed);

    const double t = kneeV(), e = endV();

    if (t < 0.99)
    {
        juce::Path gr;
        gr.startNewSubPath (xFor (t), yFor (t));
        gr.lineTo (xFor (1.0), yFor (1.0));
        gr.lineTo (xFor (1.0), yFor (e));
        gr.closeSubPath();
        g.setColour (c.withAlpha (0.22f));
        g.fillPath (gr);
    }

    juce::Path curve;
    curve.startNewSubPath (xFor (0.0), yFor (0.0));
    curve.lineTo (xFor (t), yFor (t));
    curve.lineTo (xFor (1.0), yFor (e));
    g.setColour (c);
    g.strokePath (curve, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto dot = [&] (juce::Point<float> at, float r, bool filled)
    {
        g.setColour (col::bg);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f + 3.0f, r * 2.0f + 3.0f).withCentre (at));
        g.setColour (c);
        if (filled) g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (at));
        else        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (at), 1.8f);
    };
    dot ({ xFor (t), yFor (t) }, 5.0f, true);
    if (t < 0.97)
        dot ({ xFor (1.0), yFor (e) }, 4.5f, false);

    g.setColour (col::textDim.withAlpha (0.75f));
    g.setFont (font (10.0f, true).withExtraKerningFactor (0.12f));
    g.drawText ("IN", juce::Rectangle<float> (pl.getRight() - 44.0f, pl.getBottom() + 3.0f, 44.0f, 13.0f).toNearestInt(),
                juce::Justification::centredRight);
    g.drawText ("OUT", juce::Rectangle<float> (pl.getX() + 5.0f, pl.getY() + 4.0f, 44.0f, 13.0f).toNearestInt(),
                juce::Justification::centredLeft);
    g.setColour (col::textDim.withAlpha (0.75f));
    g.setFont (font (10.0f));
    g.drawText ("shape only", juce::Rectangle<float> (pl.getX(), pl.getBottom() + 3.0f, 80.0f, 13.0f).toNearestInt(),
                juce::Justification::centredLeft);
}

//==============================================================================
CompEqGraph::CompEqGraph (MC909EditorProcessor& p)
    : ParamControl (p),
      lowFreq (findParam ("comp.low_freq")), lowGain (findParam ("comp.low_gain")),
      highFreq (findParam ("comp.high_freq")), highGain (findParam ("comp.high_gain"))
{
}

double CompEqGraph::shelfDb (double hz, double fc, double gainDb, bool lowShelf)
{
    constexpr double fs = 48000.0, pi = juce::MathConstants<double>::pi;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * pi * juce::jmin (fc, fs * 0.45) / fs;
    const double cw = std::cos (w0), alpha = std::sin (w0) / std::sqrt (2.0);   // slope 1
    const double tsa = 2.0 * std::sqrt (A) * alpha;

    double b0, b1, b2, a0, a1, a2;
    if (lowShelf)
    {
        b0 = A * ((A + 1) - (A - 1) * cw + tsa);
        b1 = 2 * A * ((A - 1) - (A + 1) * cw);
        b2 = A * ((A + 1) - (A - 1) * cw - tsa);
        a0 = (A + 1) + (A - 1) * cw + tsa;
        a1 = -2 * ((A - 1) + (A + 1) * cw);
        a2 = (A + 1) + (A - 1) * cw - tsa;
    }
    else
    {
        b0 = A * ((A + 1) + (A - 1) * cw + tsa);
        b1 = -2 * A * ((A - 1) + (A + 1) * cw);
        b2 = A * ((A + 1) + (A - 1) * cw - tsa);
        a0 = (A + 1) - (A - 1) * cw + tsa;
        a1 = 2 * ((A - 1) - (A + 1) * cw);
        a2 = (A + 1) - (A - 1) * cw - tsa;
    }

    const double w = 2.0 * pi * hz / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    const double num = std::abs (b0 + b1 * z1 + b2 * z2), den = std::abs (a0 + a1 * z1 + a2 * z2);
    return 20.0 * std::log10 (juce::jmax (num / juce::jmax (den, 1.0e-12), 1.0e-9));
}

juce::Rectangle<float> CompEqGraph::plot() const
{
    auto b = getLocalBounds().toFloat();
    return { b.getX() + 36.0f, b.getY() + 10.0f, b.getWidth() - 36.0f - 14.0f, b.getHeight() - 10.0f - 22.0f };
}

float CompEqGraph::xForHz (double hz) const
{
    const auto p = plot();
    return p.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * p.getWidth();
}

double CompEqGraph::hzForX (float x) const
{
    const auto p = plot();
    return 20.0 * std::pow (1000.0, (double) ((x - p.getX()) / p.getWidth()));
}

float CompEqGraph::yForDb (double db) const
{
    const auto p = plot();
    return p.getCentreY() - (float) (db / 16.0) * (p.getHeight() * 0.5f);
}

double CompEqGraph::dbForY (float y) const
{
    const auto p = plot();
    return (double) ((p.getCentreY() - y) / (p.getHeight() * 0.5f)) * 16.0;
}

double CompEqGraph::cornerHz (int which) const
{
    return (double) (which == 0 ? choiceHz (lowFreq, proc.getParameterValue (lowFreq))
                                : choiceHz (highFreq, proc.getParameterValue (highFreq)));
}

double CompEqGraph::gainDb (int which) const
{
    const auto& d = which == 0 ? lowGain : highGain;
    return (double) d.toDisplay (proc.getParameterValue (d));
}

double CompEqGraph::totalDb (double hz) const
{
    return shelfDb (hz, cornerHz (0), gainDb (0), true) + shelfDb (hz, cornerHz (1), gainDb (1), false);
}

juce::Point<float> CompEqGraph::handlePoint (int which) const
{
    const double fc = cornerHz (which);
    return { xForHz (fc), yForDb (juce::jlimit (-24.0, 24.0, totalDb (fc))) };
}

int CompEqGraph::nearestHandle (juce::Point<float> pos) const
{
    float best = 1.0e9f;
    int pick = -1;
    for (int w = 0; w < 2; ++w)
    {
        const float dist = pos.getDistanceFrom (handlePoint (w));
        if (dist < best) { best = dist; pick = w; }
    }
    if (best <= 20.0f)
        return pick;

    // otherwise whichever shelf the pointer is on the side of
    return std::abs (std::log (hzForX (pos.x) / cornerHz (0))) <= std::abs (std::log (hzForX (pos.x) / cornerHz (1))) ? 0 : 1;
}

void CompEqGraph::apply (int which, juce::Point<float> pos)
{
    const auto& f = which == 0 ? lowFreq : highFreq;
    const auto& g = which == 0 ? lowGain : highGain;

    const int fi = nearestChoice (f, hzForX (pos.x));
    if (fi != proc.getParameterValue (f))
        proc.setParameterValue (f, fi);

    // The handle sits on the curve, where a shelf has reached half its gain. So the height of the pointer
    // is the wanted total at the corner; take off what the other shelf contributes there and double it.
    const double fc = cornerHz (which);
    const double other = which == 0 ? shelfDb (fc, cornerHz (1), gainDb (1), false)
                                    : shelfDb (fc, cornerHz (0), gainDb (0), true);
    const int gi = juce::jlimit (0, 30, (int) std::lround (2.0 * (dbForY (pos.y) - other)) + 15);
    if (gi != proc.getParameterValue (g))
        proc.setParameterValue (g, gi);
}

void CompEqGraph::mouseMove (const juce::MouseEvent& e)
{
    const int h = nearestHandle (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
}

void CompEqGraph::mouseDown (const juce::MouseEvent& e)
{
    drag = nearestHandle (e.position);
    apply (drag, e.position);
    repaint();
}

void CompEqGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (drag >= 0)
        apply (drag, e.position);
}

void CompEqGraph::tick()
{
    // instant rise, slow fall, so peaks are readable at 30 fps
    for (int k = 0; k < AudioAnalyser::kBins; ++k)
    {
        const float raw = proc.analyser.spectrumDb (k);
        auto& shown = shownSpectrum[(size_t) k];
        shown = raw > shown ? raw : juce::jmax (raw, shown - 1.2f);
    }
    repaint();
}

void CompEqGraph::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> p) const
{
    const double sr = proc.analyser.sampleRate();
    constexpr int kN = AudioAnalyser::kFftSize, kBins = AudioAnalyser::kBins;
    if (sr <= 0.0 || ! proc.analyser.hasSignal())
        return;

    juce::Path line;
    bool started = false;
    for (float x = p.getX(); x <= p.getRight() + 0.5f; x += 2.0f)
    {
        const double hz0 = hzForX (x), hz1 = hzForX (juce::jmin (x + 2.0f, p.getRight()));
        const double b0 = hz0 * kN / sr, b1 = hz1 * kN / sr;

        float db;
        if (b1 - b0 < 1.0)                       // low end: fewer than one FFT bin per pixel, so interpolate
        {
            const double bc = 0.5 * (b0 + b1);
            const int i = juce::jlimit (0, kBins - 2, (int) std::floor (bc));
            const float f = (float) juce::jlimit (0.0, 1.0, bc - (double) i);
            db = shownSpectrum[(size_t) i] * (1.0f - f) + shownSpectrum[(size_t) (i + 1)] * f;
        }
        else                                     // high end: many bins per pixel, show the loudest
        {
            const int i0 = juce::jlimit (0, kBins - 1, (int) std::floor (b0));
            const int i1 = juce::jlimit (0, kBins - 1, (int) std::ceil (b1));
            db = AudioAnalyser::kFloorDb;
            for (int i = i0; i <= i1; ++i)
                db = juce::jmax (db, shownSpectrum[(size_t) i]);
        }

        const double tilt = 4.5 * std::log2 (std::sqrt (hz0 * hz1) / 1000.0);   // 4.5 dB / octave, like analysers
        const float frac = juce::jlimit (0.0f, 1.0f, (db + (float) tilt + 100.0f) / 100.0f);
        const float y = p.getBottom() - frac * p.getHeight();

        if (! started) { line.startNewSubPath (x, y); started = true; }
        else           line.lineTo (x, y);
    }

    juce::Path fill (line);
    fill.lineTo (p.getRight(), p.getBottom());
    fill.lineTo (p.getX(), p.getBottom());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.14f), 0.0f, p.getY(),
                                             juce::Colours::white.withAlpha (0.02f), 0.0f, p.getBottom(), false));
    g.fillPath (fill);
    g.setColour (juce::Colours::white.withAlpha (0.30f));
    g.strokePath (line, juce::PathStrokeType (1.1f));
}

void CompEqGraph::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = findColour (accentColourId, true);
    const auto p = plot();

    g.setColour (col::well);
    g.fillRoundedRectangle (b, 6.0f);

    g.setFont (font (10.0f));
    for (int db = -15; db <= 15; db += 5)
    {
        const float y = yForDb (db);
        g.setColour (col::cardEdge.withAlpha (db == 0 ? 1.0f : 0.5f));
        g.fillRect (p.getX(), y, p.getWidth(), 1.0f);
        g.setColour (col::textDim.withAlpha (0.8f));
        g.drawText (db > 0 ? "+" + juce::String (db) : juce::String (db),
                    juce::Rectangle<float> (b.getX() + 2.0f, y - 6.0f, 30.0f, 12.0f).toNearestInt(), juce::Justification::centredRight);
    }

    for (const double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xForHz (hz);
        g.setColour (col::cardEdge.withAlpha (0.5f));
        g.fillRect (x, p.getY(), 1.0f, p.getHeight());
        g.setColour (col::textDim.withAlpha (0.8f));
        g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 20.0f, p.getBottom() + 4.0f, 40.0f, 14.0f).toNearestInt(), juce::Justification::centred);
    }

    paintSpectrum (g, p);

    juce::Path line;
    const int steps = juce::jmax (16, (int) p.getWidth() / 2);
    for (int i = 0; i <= steps; ++i)
    {
        const float x = p.getX() + p.getWidth() * (float) i / (float) steps;
        const double hz = hzForX (x);
        const double db = juce::jlimit (-24.0, 24.0, totalDb (hz));
        if (i == 0) line.startNewSubPath (x, yForDb (db));
        else        line.lineTo (x, yForDb (db));
    }

    juce::Path fill (line);
    fill.lineTo (p.getRight(), yForDb (0.0));
    fill.lineTo (p.getX(), yForDb (0.0));
    fill.closeSubPath();
    g.setColour (c.withAlpha (0.18f));
    g.fillPath (fill);
    g.setColour (c);
    g.strokePath (line, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int w = 0; w < 2; ++w)
    {
        const auto at = handlePoint (w);
        const bool hot = hover == w || drag == w;

        g.setColour (col::bg);
        g.fillEllipse (juce::Rectangle<float> (hot ? 19.0f : 16.0f, hot ? 19.0f : 16.0f).withCentre (at));
        g.setColour (hot ? c.brighter (0.3f) : c);
        g.fillEllipse (juce::Rectangle<float> (hot ? 13.0f : 10.0f, hot ? 13.0f : 10.0f).withCentre (at));

        const juce::String label = hzText (cornerHz (w)) + "   " + dbText ((int) gainDb (w));
        const float lw = 116.0f;
        auto box = juce::Rectangle<float> (lw, 18.0f).withCentre ({ juce::jlimit (p.getX() + lw * 0.5f, p.getRight() - lw * 0.5f, at.x),
                                                                   at.y + (gainDb (w) >= 0.0 ? -24.0f : 24.0f) });
        box = box.constrainedWithin (p.expanded (0.0f, 6.0f));
        g.setColour (col::bg.withAlpha (0.88f));
        g.fillRoundedRectangle (box, 9.0f);
        g.setColour (c.withAlpha (0.9f));
        g.drawRoundedRectangle (box.reduced (0.5f), 9.0f, 1.0f);
        g.setColour (col::text);
        g.setFont (font (10.5f, true));
        g.drawText (label, box.toNearestInt(), juce::Justification::centred);
    }
}

//==============================================================================
namespace
{
    constexpr float kMeterFloor = -60.0f;
    float linToDb (float lin) { return lin < 1.0e-6f ? AudioAnalyser::kFloorDb : 20.0f * std::log10 (lin); }
    float meterFrac (float db) { return juce::jlimit (0.0f, 1.0f, (db - kMeterFloor) / -kMeterFloor); }
}

CompLevelMeter::CompLevelMeter (MC909EditorProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
}

void CompLevelMeter::tick()
{
    haveSignal = proc.analyser.hasSignal();
    for (int ch = 0; ch < 2; ++ch)
    {
        const float db = linToDb (proc.analyser.takePeak (ch));
        peakDb[ch] = db >= peakDb[ch] ? db : juce::jmax (db, peakDb[ch] - 1.6f);

        if (db >= holdDb[ch])        { holdDb[ch] = db; holdTicks[ch] = 40; }
        else if (holdTicks[ch] > 0)  --holdTicks[ch];
        else                         holdDb[ch] = juce::jmax (db, holdDb[ch] - 1.0f);
    }
    repaint();
}

void CompLevelMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (col::well);
    g.fillRoundedRectangle (b, 6.0f);

    g.setColour (col::textDim);
    g.setFont (font (10.0f, true).withExtraKerningFactor (0.14f));
    g.drawText ("RETURN LEVEL", juce::Rectangle<float> (b.getX() + 10.0f, b.getY() + 5.0f, 120.0f, 13.0f).toNearestInt(),
                juce::Justification::centredLeft);

    const float barX = b.getX() + 28.0f, barW = b.getWidth() - 28.0f - 46.0f;
    const juce::Colour hi (0xffff5a5a);

    for (int ch = 0; ch < 2; ++ch)
    {
        const juce::Rectangle<float> bar (barX, b.getY() + 22.0f + (float) ch * 18.0f, barW, 12.0f);
        g.setColour (juce::Colour (0xff181b21));
        g.fillRoundedRectangle (bar, 2.0f);

        const float w = meterFrac (peakDb[ch]) * bar.getWidth();
        if (w > 0.5f)
        {
            juce::ColourGradient grad (juce::Colour (0xff35e0c8), bar.getX(), 0.0f, hi, bar.getRight(), 0.0f, false);
            grad.addColour (0.80, juce::Colour (0xff6af0a8));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar.getX(), bar.getY(), w, bar.getHeight(), 2.0f);
        }

        if (holdDb[ch] > kMeterFloor)
        {
            const float x = bar.getX() + meterFrac (holdDb[ch]) * bar.getWidth();
            g.setColour (holdDb[ch] >= -0.3f ? hi : juce::Colours::white.withAlpha (0.9f));
            g.fillRect (x - 1.0f, bar.getY(), 2.0f, bar.getHeight());
        }

        g.setColour (col::textDim.withAlpha (0.8f));
        g.setFont (font (9.5f, true));
        g.drawText (ch == 0 ? "L" : "R", juce::Rectangle<float> (b.getX() + 8.0f, bar.getY(), 14.0f, bar.getHeight()).toNearestInt(),
                    juce::Justification::centred);

        g.setColour (holdDb[ch] >= -0.3f ? hi : col::textDim);
        g.drawText (holdDb[ch] > kMeterFloor ? juce::String (holdDb[ch], 1) : juce::String ("-"),
                    juce::Rectangle<float> (bar.getRight() + 3.0f, bar.getY(), 40.0f, bar.getHeight()).toNearestInt(),
                    juce::Justification::centredLeft);
    }

    // scale under the bars
    g.setFont (font (8.5f));
    for (const int db : { -48, -36, -24, -12, -6, 0 })
    {
        const float x = barX + meterFrac ((float) db) * barW;
        g.setColour (col::cardEdge.withAlpha (0.7f));
        g.fillRect (x, b.getY() + 21.0f, 1.0f, 33.0f);
        g.setColour (col::textDim.withAlpha (0.8f));
        g.drawText (juce::String (db), juce::Rectangle<float> (x - 14.0f, b.getY() + 56.0f, 28.0f, 11.0f).toNearestInt(),
                    juce::Justification::centred);
    }

    g.setColour (col::textDim.withAlpha (0.75f));
    g.setFont (font (9.0f));
    juce::String caption = haveSignal ? juce::String ("dBFS, whole mix, after the unit's compressor")
                         : MC909EditorProcessor::isInstrumentBuild ? juce::String ("no audio: set Audio From to the return")
                                                                   : juce::String ("no audio arriving: route the MC-909 through this track");
    if (! haveSignal && MC909EditorProcessor::isInstrumentBuild)
    {
        const int sc = proc.sidechainState();
        caption += sc < 0 ? " [sidechain: waiting]" : sc == 0 ? " [sidechain: OFF]" : " [sidechain: on, " + juce::String (sc) + " ch]";
    }
    g.drawText (caption,
                juce::Rectangle<float> (b.getX() + 10.0f, b.getBottom() - 15.0f, b.getWidth() - 14.0f, 12.0f).toNearestInt(),
                juce::Justification::centredLeft);
}

//==============================================================================
CompEqPage::CompEqPage (MC909EditorProcessor& p) : PageBase (p)
{
    onOff = add<ParamToggle> ("fx.comp_sw", "Compressor");
    onOff->setCompact (true);

    curve = add<CompSchematic>();
    attack        = add<ParamKnob> ("comp.attack", "Attack");
    release       = add<ParamKnob> ("comp.release", "Release");
    thresholdKnob = add<ParamKnob> ("comp.threshold", "Threshold");
    ratioKnob     = add<ParamKnob> ("comp.ratio", "Ratio");
    gain          = add<ParamKnob> ("comp.gain", "Out Gain");
    gain->setValueFormatter ([] (int v) { return dbText (v); });

    eq       = add<CompEqGraph>();
    lowFreq  = add<ParamChoice> ("comp.low_freq", "Low Freq");
    lowGain  = add<ParamKnob>   ("comp.low_gain", "Low Gain");
    highFreq = add<ParamChoice> ("comp.high_freq", "High Freq");
    highGain = add<ParamKnob>   ("comp.high_gain", "High Gain");
    lowGain->setValueFormatter  ([] (int v) { return dbText (v); });
    highGain->setValueFormatter ([] (int v) { return dbText (v); });

    level   = add<ParamKnob>   ("comp.level", "Level");
    revSend = add<ParamKnob>   ("comp.rev_send", "Reverb Send");
    output  = add<ParamChoice> ("comp.output", "Output");

    meter = std::make_unique<CompLevelMeter> (p);
    addAndMakeVisible (*meter);
}

void CompEqPage::visibilityChanged()
{
    if (isShowing()) startTimerHz (30);
    else             stopTimer();
}

void CompEqPage::timerCallback()
{
    eq->tick();
    meter->tick();
}

void CompEqPage::resized()
{
    cards.clear();

    auto r = getLocalBounds().reduced (6);
    constexpr int gap = 8, bottomH = 126;

    auto bottom = r.removeFromBottom (bottomH);
    r.removeFromBottom (gap);

    const int compW = (int) ((r.getWidth() - gap) * 0.42f);
    const auto compArea = r.removeFromLeft (compW);
    r.removeFromLeft (gap);
    const auto eqArea = r;

    {
        cards.push_back ({ compArea, "Compressor" });
        onOff->setBounds (compArea.getRight() - 96, compArea.getY() + 3, 84, 24);

        auto inner = contentOf (cards.back());
        auto knobs = inner.removeFromBottom (ParamKnob::cellHeight);
        inner.removeFromBottom (4);
        compNote = inner.removeFromBottom (28);
        inner.removeFromBottom (4);
        curve->setBounds (inner);

        const int cell = knobs.getWidth() / 5;
        ParamKnob* ks[5] { attack, release, thresholdKnob, ratioKnob, gain };
        for (int i = 0; i < 5; ++i)
            ks[i]->setBounds (knobs.getX() + i * cell, knobs.getY(), cell, knobs.getHeight());
    }

    {
        cards.push_back ({ eqArea, "2-band EQ" });
        auto inner = contentOf (cards.back());
        auto row = inner.removeFromBottom (ParamKnob::cellHeight);
        inner.removeFromBottom (6);
        eq->setBounds (inner);

        const int half = row.getWidth() / 2;
        auto left = row.removeFromLeft (half), right = row;
        lowFreq->setBounds  (left.removeFromLeft (120).withTrimmedTop (22).withHeight (42));
        lowGain->setBounds  (left.removeFromLeft (ParamKnob::cellWidth + 20).withHeight (ParamKnob::cellHeight));
        highFreq->setBounds (right.removeFromLeft (120).withTrimmedTop (22).withHeight (42));
        highGain->setBounds (right.removeFromLeft (ParamKnob::cellWidth + 20).withHeight (ParamKnob::cellHeight));
    }

    {
        cards.push_back ({ bottom, "Output" });
        auto inner = contentOf (cards.back());
        level->setBounds   (inner.removeFromLeft (ParamKnob::cellWidth + 20).withHeight (ParamKnob::cellHeight));
        revSend->setBounds (inner.removeFromLeft (ParamKnob::cellWidth + 20).withHeight (ParamKnob::cellHeight));
        inner.removeFromLeft (12);
        output->setBounds  (inner.removeFromLeft (170).withTrimmedTop (20).withHeight (42));
        inner.removeFromLeft (24);
        meter->setBounds   (inner.removeFromLeft (290).withHeight (86));
        inner.removeFromLeft (28);
        outNote = inner.withTrimmedRight (6);
    }
}

void CompEqPage::paint (juce::Graphics& g)
{
    PageBase::paint (g);

    g.setColour (col::textDim.withAlpha (0.85f));
    g.setFont (font (10.0f));
    g.drawFittedText ("Attack 0.05-50 ms, Release 0.05-2000 ms, Threshold 0-127, Ratio 1:1 to infinity. "
                      "Roland does not publish what each step is worth, so the curve shows the shape only.",
                      compNote, juce::Justification::centredLeft, 2);

    g.setColour (col::text.withAlpha (0.85f));
    g.setFont (font (13.0f));
    g.drawFittedText ("The compressor and EQ are a single processor shared by the whole unit. To send a part through it, "
                      "set that part's Output Select to COMP (Part tab).",
                      outNote, juce::Justification::centredLeft, 5, 0.9f);
}
