#include "MasterPage.h"
#include <cmath>

using namespace ui;

//==============================================================================
namespace mastering
{
    static const double kRatios[14] { 1.0, 1.1, 1.2, 1.4, 1.6, 1.8, 2.0, 2.5, 3.2, 4.0, 5.6, 8.0, 16.0, 0.0 };
    static const char* const kRatioText[14] { "1:1", "1:1.1", "1:1.2", "1:1.4", "1:1.6", "1:1.8", "1:2",
                                              "1:2.5", "1:3.2", "1:4", "1:5.6", "1:8", "1:16", "1:INF" };

    double ratioValue (int step) { return kRatios[juce::jlimit (0, 13, step)]; }   // 0 means infinity
    juce::String ratioText (int step) { return kRatioText[juce::jlimit (0, 13, step)]; }

    double releaseMs (int step)
    {
        return 50.0 * std::pow (100.0, (double) juce::jlimit (0, 100, step) / 100.0);
    }

    juce::String releaseText (int step)
    {
        const double ms = releaseMs (step);
        return ms >= 1000.0 ? "~" + juce::String (ms / 1000.0, 1) + " s"
                            : "~" + juce::String ((int) std::lround (ms)) + " ms";
    }

    /** Output level for a given input level (both dB), before the band's output Level is added. */
    static double compOut (double inDb, double thresholdDb, int ratioStep)
    {
        if (inDb <= thresholdDb)
            return inDb;
        const double r = ratioValue (ratioStep);
        return r <= 0.0 ? thresholdDb : thresholdDb + (inDb - thresholdDb) / r;
    }
}

namespace
{
    const char* const kBandName[3]  { "LOW", "MID", "HIGH" };
    const char* const kBandTag[3]   { "low", "mid", "high" };
    const char* const kBandTitle[3] { "Low band", "Mid band", "High band" };

    juce::String hzText (double hz)
    {
        if (hz < 1000.0)
            return juce::String ((int) std::lround (hz)) + " Hz";
        juce::String s (hz / 1000.0, 2);
        if (s.contains ("."))
            s = s.trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
        return s + " kHz";
    }

    juce::String dbText (int db)
    {
        return (db > 0 ? "+" : "") + juce::String (db) + " dB";
    }

    float smooth (double t)
    {
        t = juce::jlimit (0.0, 1.0, t);
        return (float) (t * t * (3.0 - 2.0 * t));
    }

    void drawCard (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour dot)
    {
        const auto r = area.toFloat();
        g.setColour (col::card);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::cardEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (dot);
        g.fillEllipse (r.getX() + 12.0f, r.getY() + 11.0f, 6.0f, 6.0f);
        g.setColour (col::textDim);
        g.setFont (font (11.0f, true).withExtraKerningFactor (0.14f));
        g.drawText (title.toUpperCase(), (int) r.getX() + 24, (int) r.getY() + 6, area.getWidth() - 30, 16,
                    juce::Justification::centredLeft);
    }
}

//==============================================================================
MasterBandGraph::MasterBandGraph (MC909EditorProcessor& p)
    : ParamControl (p),
      sw (findParam ("mst.switch")),
      splitLow (findParam ("mst.split_low")),
      splitHigh (findParam ("mst.split_high"))
{
    for (int b = 0; b < 3; ++b)
    {
        const juce::String t = juce::String ("mst.") + kBandTag[b] + ".";
        level[b]     = &findParam (t + "level");
        threshold[b] = &findParam (t + "threshold");
        ratio[b]     = &findParam (t + "ratio");
        attack[b]    = &findParam (t + "attack");
    }
}

juce::Rectangle<float> MasterBandGraph::plot() const
{
    auto b = getLocalBounds().toFloat();
    return { b.getX() + 40.0f, b.getY() + 46.0f, b.getWidth() - 40.0f - 16.0f, b.getHeight() - 46.0f - 22.0f };
}

float MasterBandGraph::xForHz (double hz) const
{
    const auto p = plot();
    return p.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * p.getWidth();
}

double MasterBandGraph::hzForX (float x) const
{
    const auto p = plot();
    return 20.0 * std::pow (1000.0, (double) ((x - p.getX()) / p.getWidth()));
}

float MasterBandGraph::yForDb (double db) const
{
    const auto p = plot();
    return p.getBottom() - (float) (db / 24.0) * p.getHeight();
}

double MasterBandGraph::dbForY (float y) const
{
    const auto p = plot();
    return (double) ((p.getBottom() - y) / p.getHeight()) * 24.0;
}

double MasterBandGraph::splitHz (int which) const
{
    const auto& d = which == 0 ? splitLow : splitHigh;
    const int idx = juce::jlimit (0, d.choices.size() - 1, proc.getParameterValue (d));
    return (double) d.choices[idx].getIntValue();
}

double MasterBandGraph::bandLevel (int band) const
{
    return (double) proc.getParameterValue (*level[band]);
}

double MasterBandGraph::responseDb (double hz) const
{
    const double u = std::log10 (hz);
    const double c1 = std::log10 (splitHz (0)), c2 = std::log10 (splitHz (1));
    const double w1 = smooth ((u - (c1 - 0.09)) / 0.18);
    const double w2 = smooth ((u - (c2 - 0.09)) / 0.18);
    return bandLevel (0) * (1.0 - w1) + bandLevel (1) * w1 * (1.0 - w2) + bandLevel (2) * w2;
}

int MasterBandGraph::bandAt (float x) const
{
    if (x < xForHz (splitHz (0))) return 0;
    if (x < xForHz (splitHz (1))) return 1;
    return 2;
}

int MasterBandGraph::splitNear (float x) const
{
    for (int s = 0; s < 2; ++s)
        if (std::abs (x - xForHz (splitHz (s))) <= 9.0f)
            return s;
    return -1;
}

void MasterBandGraph::setSplit (int which, float x)
{
    const auto& d = which == 0 ? splitLow : splitHigh;
    const double hz = hzForX (x);

    int best = 0;
    double bestDist = 1.0e9;
    for (int i = 0; i < d.choices.size(); ++i)
    {
        const double dist = std::abs (std::log (hz / (double) d.choices[i].getIntValue()));
        if (dist < bestDist) { bestDist = dist; best = i; }
    }

    if (best != proc.getParameterValue (d))
        proc.setParameterValue (d, best);
}

void MasterBandGraph::setLevel (int band, float y)
{
    const int raw = juce::jlimit (0, 24, (int) std::lround (dbForY (y)));
    if (raw != proc.getParameterValue (*level[band]))
        proc.setParameterValue (*level[band], raw);
}

void MasterBandGraph::tick()
{
    auto& a = proc.analyser;
    a.setCrossovers (splitHz (0), splitHz (1));
    haveSignal = a.hasSignal();

    // instant rise, slow fall, so peaks are readable at 30 fps
    for (int k = 0; k < AudioAnalyser::kBins; ++k)
    {
        const float raw = a.spectrumDb (k);
        auto& shown = shownSpectrum[(size_t) k];
        shown = raw > shown ? raw : juce::jmax (raw, shown - 1.2f);
    }
    repaint();
}

void MasterBandGraph::paintSpectrum (juce::Graphics& g, juce::Rectangle<float> p) const
{
    const double sr = proc.analyser.sampleRate();
    constexpr int kN = AudioAnalyser::kFftSize, kBins = AudioAnalyser::kBins;

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

        // 4.5 dB per octave tilt, as spectrum analysers do, so music looks level instead of sloping away
        const double tilt = 4.5 * std::log2 (std::sqrt (hz0 * hz1) / 1000.0);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + (float) tilt + 100.0f) / 100.0f);
        const float y = p.getBottom() - frac * p.getHeight();

        if (! started) { line.startNewSubPath (x, y); started = true; }
        else           line.lineTo (x, y);
    }

    juce::Path fill (line);
    fill.lineTo (p.getRight(), p.getBottom());
    fill.lineTo (p.getX(), p.getBottom());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), 0.0f, p.getY(),
                                             juce::Colours::white.withAlpha (0.02f), 0.0f, p.getBottom(), false));
    g.fillPath (fill);
    g.setColour (juce::Colours::white.withAlpha (0.38f));
    g.strokePath (line, juce::PathStrokeType (1.2f));
}

void MasterBandGraph::mouseMove (const juce::MouseEvent& e)
{
    const int s = splitNear (e.position.x);
    const int b = s >= 0 ? -1 : bandAt (e.position.x);
    if (s != hoverSplit || b != hoverBand)
    {
        hoverSplit = s;
        hoverBand = b;
        repaint();
    }
    setMouseCursor (s >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
}

void MasterBandGraph::mouseExit (const juce::MouseEvent&)
{
    hoverSplit = hoverBand = -1;
    repaint();
}

void MasterBandGraph::mouseDown (const juce::MouseEvent& e)
{
    dragSplit = splitNear (e.position.x);
    dragBand = dragSplit >= 0 ? -1 : bandAt (e.position.x);
    if (dragBand >= 0)
        setLevel (dragBand, e.position.y);
    repaint();
}

void MasterBandGraph::mouseDrag (const juce::MouseEvent& e)
{
    if (dragSplit >= 0)
        setSplit (dragSplit, e.position.x);
    else if (dragBand >= 0)
        setLevel (dragBand, e.position.y);
}

void MasterBandGraph::mouseUp (const juce::MouseEvent&)
{
    dragSplit = dragBand = -1;
    repaint();
}

const mc909::ParamDef* MasterBandGraph::teachDef (juce::Point<int> p) const
{
    const int s = splitNear ((float) p.x);
    if (s >= 0)
        return s == 0 ? &splitLow : &splitHigh;
    return level[bandAt ((float) p.x)];
}

void MasterBandGraph::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const bool on = proc.getParameterValue (sw) != 0;

    g.setColour (col::well);
    g.fillRoundedRectangle (b, 6.0f);

    if (! on)
        g.beginTransparencyLayer (0.42f);

    const auto p = plot();

    // grid: level (dB) horizontally, frequency vertically
    g.setFont (font (10.0f));
    for (int db = 0; db <= 24; db += 6)
    {
        const float y = yForDb (db);
        g.setColour (col::cardEdge.withAlpha (db == 0 ? 1.0f : 0.55f));
        g.fillRect (p.getX(), y, p.getWidth(), 1.0f);
        g.setColour (col::textDim.withAlpha (0.8f));
        g.drawText (juce::String (db), juce::Rectangle<float> (b.getX() + 4.0f, y - 7.0f, 30.0f, 14.0f).toNearestInt(),
                    juce::Justification::centredRight);
    }

    for (const double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xForHz (hz);
        g.setColour (col::cardEdge.withAlpha (0.55f));
        g.fillRect (x, p.getY(), 1.0f, p.getHeight());
        g.setColour (col::textDim.withAlpha (0.8f));
        g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x - 20.0f, p.getBottom() + 4.0f, 40.0f, 14.0f).toNearestInt(),
                    juce::Justification::centred);
    }
    g.setColour (col::textDim.withAlpha (0.7f));
    g.drawText ("dB", juce::Rectangle<float> (b.getX() + 6.0f, p.getY() - 24.0f, 28.0f, 12.0f).toNearestInt(),
                juce::Justification::centredRight);

    if (haveSignal)
        paintSpectrum (g, p);
    else
    {
        g.setColour (col::textDim.withAlpha (0.75f));
        g.setFont (font (11.0f));
        g.drawText ("No audio reaching the plugin: route the MC-909's return through this track for a live spectrum and meters",
                    juce::Rectangle<float> (p.getX(), p.getY() + 6.0f, p.getWidth(), 16.0f).toNearestInt(),
                    juce::Justification::centred);
    }

    // the blended level curve, filled and stroked one band at a time so each band keeps its colour
    const double s0 = splitHz (0), s1 = splitHz (1);
    const float xs[4] { p.getX(), xForHz (s0), xForHz (s1), p.getRight() };

    juce::Path line;
    const int steps = juce::jmax (16, (int) p.getWidth() / 2);
    for (int i = 0; i <= steps; ++i)
    {
        const float x = p.getX() + p.getWidth() * (float) i / (float) steps;
        const float y = yForDb (responseDb (hzForX (x)));
        if (i == 0) line.startNewSubPath (x, y);
        else        line.lineTo (x, y);
    }

    juce::Path fill (line);
    fill.lineTo (p.getRight(), p.getBottom());
    fill.lineTo (p.getX(), p.getBottom());
    fill.closeSubPath();

    for (int band = 0; band < 3; ++band)
    {
        const auto c = mastering::kBandColour[band];
        const bool hot = (hoverBand == band) || (dragBand == band);

        g.saveState();
        g.reduceClipRegion (juce::Rectangle<float> (xs[band], p.getY() - 2.0f, xs[band + 1] - xs[band], p.getHeight() + 4.0f).toNearestInt());

        g.setGradientFill (juce::ColourGradient (c.withAlpha (hot ? 0.42f : 0.30f), 0.0f, yForDb (bandLevel (band)),
                                                 c.withAlpha (0.04f), 0.0f, p.getBottom(), false));
        g.fillPath (fill);

        g.setColour (c);
        g.strokePath (line, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.restoreState();
    }

    // per-band heading and settings, level pill on the line
    for (int band = 0; band < 3; ++band)
    {
        const auto c = mastering::kBandColour[band];
        const float cx = (xs[band] + xs[band + 1]) * 0.5f;
        const float width = xs[band + 1] - xs[band];

        g.setColour (c);
        g.setFont (font (12.0f, true).withExtraKerningFactor (0.16f));
        g.drawText (kBandName[band], juce::Rectangle<float> (cx - width * 0.5f, b.getY() + 8.0f, width, 16.0f).toNearestInt(),
                    juce::Justification::centred);

        g.setColour (col::textDim);
        g.setFont (font (10.5f));
        const auto info = "Thr " + dbText (proc.getParameterValue (*threshold[band]) - 36)
                          + "   " + mastering::ratioText (proc.getParameterValue (*ratio[band]));
        g.drawText (info, juce::Rectangle<float> (cx - width * 0.5f, b.getY() + 25.0f, width, 15.0f).toNearestInt(),
                    juce::Justification::centred);

        const juce::String lv = dbText ((int) bandLevel (band));
        const float py = juce::jmax (p.getY() + 11.0f, yForDb (bandLevel (band)) - 15.0f);
        const auto pill = juce::Rectangle<float> (48.0f, 17.0f).withCentre ({ cx, py });
        g.setColour (col::bg.withAlpha (0.88f));
        g.fillRoundedRectangle (pill, 8.5f);
        g.setColour (c.withAlpha (0.9f));
        g.drawRoundedRectangle (pill.reduced (0.5f), 8.5f, 1.0f);
        g.setColour (col::text);
        g.setFont (font (10.5f, true));
        g.drawText (lv, pill.toNearestInt(), juce::Justification::centred);
    }

    // crossover lines with a grip that carries the frequency
    for (int s = 0; s < 2; ++s)
    {
        const float x = xs[s + 1];
        const bool hot = (hoverSplit == s) || (dragSplit == s);

        g.setColour (juce::Colours::white.withAlpha (hot ? 0.85f : 0.32f));
        g.fillRect (x - 0.5f, p.getY(), hot ? 2.0f : 1.0f, p.getHeight());

        const auto grip = juce::Rectangle<float> (62.0f, 20.0f).withCentre ({ x, p.getCentreY() });
        g.setColour (hot ? col::text : col::knobBody.brighter (0.25f));
        g.fillRoundedRectangle (grip, 10.0f);
        g.setColour (hot ? col::bg : col::text);
        g.setFont (font (10.5f, true));
        g.drawText (hzText (s == 0 ? s0 : s1), grip.toNearestInt(), juce::Justification::centred);
    }

    if (! on)
    {
        g.endTransparencyLayer();
        g.setColour (col::text.withAlpha (0.75f));
        g.setFont (font (11.0f, true).withExtraKerningFactor (0.2f));
        g.drawText ("MASTERING OFF", juce::Rectangle<float> (plot().getX(), plot().getY() + 6.0f, plot().getWidth(), 16.0f).toNearestInt(),
                    juce::Justification::centred);
    }
}

//==============================================================================
namespace
{
    constexpr float kMeterFloor = -60.0f;

    float linearToDb (float lin) { return lin < 1.0e-6f ? AudioAnalyser::kFloorDb : 20.0f * std::log10 (lin); }
    float meterFrac (float db) { return juce::jlimit (0.0f, 1.0f, (db - kMeterFloor) / -kMeterFloor); }
    const juce::Colour kClip { 0xffff5a5a };

    /** Peak hold: jumps up at once, holds about a second, then falls. */
    void updateHold (float& hold, int& ticks, float db)
    {
        if (db >= hold)           { hold = db; ticks = 30; }
        else if (ticks > 0)       { --ticks; }
        else                      { hold = juce::jmax (AudioAnalyser::kFloorDb, hold - 0.8f); }
    }

    void drawScale (juce::Graphics& g, juce::Rectangle<float> bar, bool labels, juce::Rectangle<float> labelArea)
    {
        g.setFont (font (9.0f));
        for (const int db : { 0, -12, -24, -36, -48 })
        {
            const float y = bar.getBottom() - meterFrac ((float) db) * bar.getHeight();
            g.setColour (juce::Colours::white.withAlpha (0.13f));
            g.fillRect (bar.getX(), y, bar.getWidth(), 1.0f);
            if (labels)
            {
                g.setColour (col::textDim.withAlpha (0.85f));
                g.drawText (juce::String (db), juce::Rectangle<float> (labelArea.getX(), y - 6.0f, labelArea.getWidth(), 12.0f).toNearestInt(),
                            juce::Justification::centredLeft);
            }
        }
    }
}

MasterBandMeter::MasterBandMeter (MC909EditorProcessor& p, int b) : proc (p), band (b)
{
    setInterceptsMouseClicks (false, false);
}

void MasterBandMeter::tick()
{
    rmsDb = proc.analyser.bandRmsDb (band);
    updateHold (peakDb, holdTicks, linearToDb (proc.analyser.takeBandPeak (band)));
    repaint();
}

void MasterBandMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = mastering::kBandColour[band];

    g.setColour (col::well);
    g.fillRoundedRectangle (b, 5.0f);

    const auto bar = b.reduced (4.0f, 6.0f);
    drawScale (g, bar, false, {});

    const float top = bar.getBottom() - meterFrac (rmsDb) * bar.getHeight();
    if (bar.getBottom() - top > 0.5f)
    {
        g.setGradientFill (juce::ColourGradient (c.brighter (0.25f), 0.0f, bar.getY(), c.darker (0.55f), 0.0f, bar.getBottom(), false));
        g.fillRoundedRectangle (bar.getX(), top, bar.getWidth(), bar.getBottom() - top, 2.0f);
    }

    if (peakDb > kMeterFloor)
    {
        const float y = bar.getBottom() - meterFrac (peakDb) * bar.getHeight();
        g.setColour (peakDb >= -1.0f ? kClip : juce::Colours::white.withAlpha (0.9f));
        g.fillRect (bar.getX(), y - 1.0f, bar.getWidth(), 2.0f);
    }
}

//------------------------------------------------------------------------------
MasterOutputPanel::MasterOutputPanel (MC909EditorProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
}

void MasterOutputPanel::tick()
{
    haveSignal = proc.analyser.hasSignal();
    for (int ch = 0; ch < 2; ++ch)
    {
        const float db = linearToDb (proc.analyser.takePeak (ch));
        peakDb[ch] = db >= peakDb[ch] ? db : juce::jmax (db, peakDb[ch] - 1.6f);
        updateHold (holdDb[ch], holdTicks[ch], db);
    }
    repaint();
}

void MasterOutputPanel::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (col::well);
    g.fillRoundedRectangle (b, 6.0f);

    g.setColour (col::textDim);
    g.setFont (font (10.0f, true).withExtraKerningFactor (0.14f));
    g.drawText ("OUTPUT", juce::Rectangle<float> (b.getX() + 10.0f, b.getY() + 6.0f, 80.0f, 14.0f).toNearestInt(),
                juce::Justification::centredLeft);

    const float barTop = b.getY() + 28.0f, barBottom = b.getBottom() - 30.0f;
    const juce::Rectangle<float> bars (b.getX() + 10.0f, barTop, 50.0f, barBottom - barTop);
    drawScale (g, bars, true, juce::Rectangle<float> (bars.getRight() + 4.0f, 0.0f, 24.0f, 0.0f));

    for (int ch = 0; ch < 2; ++ch)
    {
        const juce::Rectangle<float> bar (bars.getX() + (float) ch * 28.0f, barTop, 22.0f, barBottom - barTop);
        g.setColour (juce::Colour (0xff181b21));
        g.fillRoundedRectangle (bar, 2.0f);

        const float top = bar.getBottom() - meterFrac (peakDb[ch]) * bar.getHeight();
        if (bar.getBottom() - top > 0.5f)
        {
            juce::ColourGradient grad (mastering::kBandColour[1], 0.0f, bar.getBottom(), kClip, 0.0f, bar.getY(), false);
            grad.addColour (0.82, mastering::kBandColour[1].brighter (0.2f));
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar.getX(), top, bar.getWidth(), bar.getBottom() - top, 2.0f);
        }

        if (holdDb[ch] > kMeterFloor)
        {
            const float y = bar.getBottom() - meterFrac (holdDb[ch]) * bar.getHeight();
            g.setColour (holdDb[ch] >= -0.3f ? kClip : juce::Colours::white.withAlpha (0.9f));
            g.fillRect (bar.getX(), y - 1.0f, bar.getWidth(), 2.0f);
        }

        g.setColour (holdDb[ch] >= -0.3f ? kClip : col::textDim);
        g.setFont (font (9.5f, true));
        g.drawText (holdDb[ch] > kMeterFloor ? juce::String (holdDb[ch], 1) : juce::String ("-"),
                    juce::Rectangle<float> (bar.getX() - 3.0f, barBottom + 4.0f, 28.0f, 12.0f).toNearestInt(),
                    juce::Justification::centred);
        g.setColour (col::textDim.withAlpha (0.7f));
        g.setFont (font (9.0f));
        g.drawText (ch == 0 ? "L" : "R", juce::Rectangle<float> (bar.getX(), barBottom + 15.0f, 22.0f, 11.0f).toNearestInt(),
                    juce::Justification::centred);
    }

    auto readout = [&] (const char* label, float lufs, float y)
    {
        const auto x = bars.getRight() + 31.0f;
        g.setColour (col::textDim);
        g.setFont (font (9.0f, true));
        g.drawText (label, juce::Rectangle<float> (x, y, b.getRight() - x - 6.0f, 12.0f).toNearestInt(), juce::Justification::centredLeft);

        const bool valid = haveSignal && lufs > -100.0f;
        g.setColour (valid ? col::text : col::textDim.withAlpha (0.6f));
        g.setFont (font (19.0f, true));
        g.drawText (valid ? juce::String (lufs, 1) : juce::String ("-"),
                    juce::Rectangle<float> (x, y + 13.0f, b.getRight() - x - 6.0f, 24.0f).toNearestInt(), juce::Justification::centredLeft);

        g.setColour (col::textDim.withAlpha (0.8f));
        g.setFont (font (9.0f));
        g.drawText ("LUFS", juce::Rectangle<float> (x, y + 37.0f, 40.0f, 11.0f).toNearestInt(), juce::Justification::centredLeft);
    };
    readout ("MOMENTARY", proc.analyser.momentaryLufs(), barTop - 2.0f);
    readout ("SHORT-TERM", proc.analyser.shortTermLufs(), barTop + 70.0f);
}

//==============================================================================
MasterCompCurve::MasterCompCurve (MC909EditorProcessor& p, int bandIndex)
    : ParamControl (p), band (bandIndex),
      threshold (findParam (juce::String ("mst.") + kBandTag[bandIndex] + ".threshold")),
      ratio (findParam (juce::String ("mst.") + kBandTag[bandIndex] + ".ratio"))
{
    setColour (accentColourId, mastering::kBandColour[bandIndex]);
}

juce::Rectangle<float> MasterCompCurve::plot() const
{
    auto b = getLocalBounds().toFloat();
    return { b.getX() + 30.0f, b.getY() + 10.0f, b.getWidth() - 30.0f - 12.0f, b.getHeight() - 10.0f - 20.0f };
}

float MasterCompCurve::xForDb (double db) const
{
    const auto p = plot();
    return p.getX() + (float) ((db + 48.0) / 48.0) * p.getWidth();
}

float MasterCompCurve::yForDb (double db) const
{
    const auto p = plot();
    return p.getBottom() - (float) ((db + 48.0) / 48.0) * p.getHeight();
}

double MasterCompCurve::dbForX (float x) const
{
    const auto p = plot();
    return (double) ((x - p.getX()) / p.getWidth()) * 48.0 - 48.0;
}

double MasterCompCurve::dbForY (float y) const
{
    const auto p = plot();
    return (double) ((p.getBottom() - y) / p.getHeight()) * 48.0 - 48.0;
}

double MasterCompCurve::thresholdDb() const { return (double) threshold.toDisplay (proc.getParameterValue (threshold)); }
int    MasterCompCurve::ratioStep() const   { return proc.getParameterValue (ratio); }

void MasterCompCurve::mouseDown (const juce::MouseEvent& e)
{
    const double t = thresholdDb();
    const float kx = xForDb (t), ky = yForDb (t);
    const float ex = xForDb (0.0), ey = yForDb (mastering::compOut (0.0, t, ratioStep()));

    const float dk = e.position.getDistanceFrom ({ kx, ky });
    const float de = e.position.getDistanceFrom ({ ex, ey });

    drag = (t < -0.5 && de < dk && de < 16.0f) ? end : knee;
    mouseDrag (e);
}

void MasterCompCurve::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == knee)
    {
        const int raw = juce::jlimit (0, 36, (int) std::lround (dbForX (e.position.x)) + 36);
        if (raw != proc.getParameterValue (threshold))
            proc.setParameterValue (threshold, raw);
    }
    else if (drag == end)
    {
        const double t = thresholdDb();
        const double want = juce::jlimit (t, 0.0, dbForY (e.position.y));

        int best = 0;
        double bestDist = 1.0e9;
        for (int i = 0; i < 14; ++i)
        {
            const double d = std::abs (mastering::compOut (0.0, t, i) - want);
            if (d < bestDist) { bestDist = d; best = i; }
        }
        if (best != ratioStep())
            proc.setParameterValue (ratio, best);
    }
}

const mc909::ParamDef* MasterCompCurve::teachDef (juce::Point<int> p) const
{
    return p.x > getWidth() * 3 / 4 ? &ratio : &threshold;
}

void MasterCompCurve::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = findColour (accentColourId, true);
    const auto pl = plot();

    g.setColour (col::well);
    g.fillRoundedRectangle (b, 6.0f);

    g.setFont (font (9.5f));
    for (const int db : { -36, -24, -12, 0 })
    {
        g.setColour (col::cardEdge.withAlpha (db == 0 ? 1.0f : 0.55f));
        g.fillRect (pl.getX(), yForDb (db), pl.getWidth(), 1.0f);
        g.fillRect (xForDb (db), pl.getY(), 1.0f, pl.getHeight());

        g.setColour (col::textDim.withAlpha (0.8f));
        g.drawText (juce::String (db), juce::Rectangle<float> (b.getX() + 1.0f, yForDb (db) - 6.0f, 26.0f, 12.0f).toNearestInt(),
                    juce::Justification::centredRight);
        g.drawText (juce::String (db), juce::Rectangle<float> (xForDb (db) - 14.0f, pl.getBottom() + 3.0f, 28.0f, 12.0f).toNearestInt(),
                    juce::Justification::centred);
    }
    g.setColour (col::textDim.withAlpha (0.6f));
    g.drawText ("IN dB", juce::Rectangle<float> (pl.getRight() - 52.0f, pl.getBottom() - 15.0f, 50.0f, 12.0f).toNearestInt(),
                juce::Justification::centredRight);
    g.drawText ("OUT", juce::Rectangle<float> (pl.getX() + 4.0f, pl.getY() + 1.0f, 40.0f, 12.0f).toNearestInt(),
                juce::Justification::centredLeft);

    // 1:1 reference
    juce::Path diag, dashed;
    diag.startNewSubPath (xForDb (-48.0), yForDb (-48.0));
    diag.lineTo (xForDb (0.0), yForDb (0.0));
    const float dashes[2] { 4.0f, 4.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, diag, dashes, 2);
    g.setColour (col::textDim.withAlpha (0.55f));
    g.fillPath (dashed);

    const double t = thresholdDb();
    const double out0 = mastering::compOut (0.0, t, ratioStep());

    // what the compressor takes off, between the 1:1 line and the real curve
    if (t < -0.01)
    {
        juce::Path gr;
        gr.startNewSubPath (xForDb (t), yForDb (t));
        gr.lineTo (xForDb (0.0), yForDb (0.0));
        gr.lineTo (xForDb (0.0), yForDb (out0));
        gr.closeSubPath();
        g.setColour (c.withAlpha (0.24f));
        g.fillPath (gr);
    }

    juce::Path curve;
    curve.startNewSubPath (xForDb (-48.0), yForDb (-48.0));
    curve.lineTo (xForDb (t), yForDb (t));
    curve.lineTo (xForDb (0.0), yForDb (out0));
    g.setColour (c);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // handles: knee = threshold, end = ratio
    auto dot = [&] (juce::Point<float> at, float r, bool filled)
    {
        g.setColour (col::bg);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f + 3.0f, r * 2.0f + 3.0f).withCentre (at));
        g.setColour (c);
        if (filled) g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (at));
        else        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (at), 1.8f);
    };
    dot ({ xForDb (t), yForDb (t) }, 4.5f, true);
    if (t < -0.5)
        dot ({ xForDb (0.0), yForDb (out0) }, 4.0f, false);

    g.setColour (col::textDim);
    g.setFont (font (10.0f));
    g.drawText ("GR at 0 dB in: " + juce::String (-out0, 1) + " dB",
                juce::Rectangle<float> (pl.getRight() - 154.0f, pl.getBottom() - 29.0f, 150.0f, 12.0f).toNearestInt(),
                juce::Justification::centredRight);
}

//==============================================================================
MasterPage::MasterPage (MC909EditorProcessor& p) : PageBase (p)
{
    onOff = add<ParamToggle> ("mst.switch", "Mastering");
    onOff->setCompact (true);
    graph = add<MasterBandGraph>();

    output = std::make_unique<MasterOutputPanel> (proc);
    addAndMakeVisible (*output);
    for (int band = 0; band < 3; ++band)
    {
        meters[band] = std::make_unique<MasterBandMeter> (proc, band);
        addAndMakeVisible (*meters[band]);
    }

    for (int band = 0; band < 3; ++band)
    {
        curves[band] = add<MasterCompCurve> (band);

        const juce::String t = juce::String ("mst.") + kBandTag[band] + ".";
        const char* const ids[5]  { "attack", "release", "threshold", "ratio", "level" };
        const char* const caps[5] { "Attack", "Release", "Threshold", "Ratio", "Level" };

        for (int k = 0; k < 5; ++k)
        {
            auto* knob = add<ParamKnob> (t + ids[k], caps[k]);
            knob->setColour (accentColourId, mastering::kBandColour[band]);
            knobs[band][k] = knob;
        }

        knobs[band][0]->setValueFormatter ([] (int v) { return juce::String (v) + " ms"; });
        knobs[band][1]->setValueFormatter ([] (int v) { return mastering::releaseText (v); });
        knobs[band][2]->setValueFormatter ([] (int v) { return juce::String (v) + " dB"; });
        knobs[band][3]->setValueFormatter ([] (int v) { return mastering::ratioText (v); });
        knobs[band][4]->setValueFormatter ([] (int v) { return dbText (v); });
    }
}

void MasterPage::resized()
{
    auto r = getLocalBounds().reduced (6);
    constexpr int gap = 8, bandH = 292;

    graphCard = r.removeFromTop (r.getHeight() - bandH - gap);
    r.removeFromTop (gap);

    {
        auto head = graphCard.reduced (10, 0).removeFromTop (28);
        onOff->setBounds (head.removeFromRight (84).withTrimmedTop (2).withHeight (24));

        auto inner = graphCard.reduced (10, 8).withTrimmedTop (28);
        output->setBounds (inner.removeFromRight (184));
        inner.removeFromRight (8);
        graph->setBounds (inner);
    }

    const int w = (r.getWidth() - 2 * gap) / 3;
    for (int band = 0; band < 3; ++band)
    {
        bandCards[band] = r.removeFromLeft (w);
        r.removeFromLeft (gap);

        auto inner = bandCards[band].reduced (10, 8).withTrimmedTop (22);
        auto row = inner.removeFromTop (inner.getHeight() - 6 - ParamKnob::cellHeight);
        meters[band]->setBounds (row.removeFromRight (24));
        row.removeFromRight (6);
        curves[band]->setBounds (row);
        inner.removeFromTop (6);

        const int cell = inner.getWidth() / 5;
        for (int k = 0; k < 5; ++k)
            knobs[band][k]->setBounds (inner.getX() + k * cell, inner.getY(), cell, inner.getHeight());
    }
}

void MasterPage::paint (juce::Graphics& g)
{
    drawCard (g, graphCard, "Multiband compressor", mastering::kBandColour[1]);

    g.setColour (col::textDim.withAlpha (0.8f));
    g.setFont (font (10.0f));
    g.drawText ("Drag a split sideways, or a band up and down.  Spectrum and meters show the audio coming back from the MC-909.",
                graphCard.getX() + 250, graphCard.getY() + 6, graphCard.getWidth() - 250 - 110, 16,
                juce::Justification::centredLeft);

    for (int band = 0; band < 3; ++band)
        drawCard (g, bandCards[band], kBandTitle[band], mastering::kBandColour[band]);
}

void MasterPage::visibilityChanged()
{
    if (isShowing()) startTimerHz (30);
    else             stopTimer();
}

void MasterPage::timerCallback()
{
    graph->tick();
    output->tick();
    for (auto& m : meters)
        m->tick();
}
