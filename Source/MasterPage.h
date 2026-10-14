#pragma once
#include "UiKit.h"

/** MASTER tab: the unit's three-band compressor drawn as a picture instead of a list of sliders.

    Everything here is derived from the parameters. There is no audio and no level metering (the
    MC-909 does not send any), so the graphs show what the settings DO, not what is passing through. */

namespace mastering
{
    const juce::Colour kBandColour[3] { juce::Colour (0xffff7a45),    // low
                                        juce::Colour (0xff35e0c8),    // mid
                                        juce::Colour (0xff7aa8ff) };  // high

    /** Compression ratio steps (parameter 0-13); the last is infinity. */
    double ratioValue (int step);
    juce::String ratioText (int step);

    /** The unit gives the release as 0-100 over 50-5000 ms. The curve between the ends is not in the
        documents, so this assumes a logarithmic one and the readout says "~". */
    double releaseMs (int step);
    juce::String releaseText (int step);
}

/** Frequency graph: three bands with their output levels and the two crossover points.
    Drag a crossover line sideways to move the split; drag inside a band up or down to set its level. */
class MasterBandGraph : public ui::ParamControl
{
public:
    explicit MasterBandGraph (MC909EditorProcessor&);

    void paint (juce::Graphics&) override;
    void refresh() override { repaint(); }

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    const mc909::ParamDef* teachDef (juce::Point<int>) const override;

private:
    juce::Rectangle<float> plot() const;
    float xForHz (double hz) const;
    double hzForX (float x) const;
    float yForDb (double db) const;
    double dbForY (float y) const;
    double splitHz (int which) const;           // 0 = low/mid, 1 = mid/high
    int    bandAt (float x) const;
    int    splitNear (float x) const;           // -1 if none close enough
    double bandLevel (int band) const;
    double responseDb (double hz) const;        // levels blended smoothly across the crossovers
    void   setSplit (int which, float x);
    void   setLevel (int band, float y);

    const mc909::ParamDef &sw, &splitLow, &splitHigh;
    const mc909::ParamDef* level[3];
    const mc909::ParamDef* threshold[3];
    const mc909::ParamDef* ratio[3];
    const mc909::ParamDef* attack[3];

    int hoverSplit = -1, hoverBand = -1;
    int dragSplit = -1, dragBand = -1;
};

/** One band's compressor as an input/output curve, with the gain taken off shaded in.
    The knee and the right-hand end of the curve can be dragged to set threshold and ratio. */
class MasterCompCurve : public ui::ParamControl
{
public:
    MasterCompCurve (MC909EditorProcessor&, int band);

    void paint (juce::Graphics&) override;
    void refresh() override { repaint(); }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = none; }

    const mc909::ParamDef* teachDef (juce::Point<int>) const override;

private:
    enum Drag { none, knee, end };

    juce::Rectangle<float> plot() const;
    float xForDb (double db) const;
    float yForDb (double db) const;
    double dbForX (float x) const;
    double dbForY (float y) const;
    double thresholdDb() const;
    int    ratioStep() const;

    const int band;
    const mc909::ParamDef &threshold, &ratio;
    Drag drag = none;
};

class MasterPage : public ui::PageBase
{
public:
    explicit MasterPage (MC909EditorProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    MasterBandGraph* graph = nullptr;
    ui::ParamToggle* onOff = nullptr;
    MasterCompCurve* curves[3] {};
    ui::ParamKnob*   knobs[3][5] {};
    juce::Rectangle<int> bandCards[3];
    juce::Rectangle<int> graphCard;
};
