#pragma once
#include "UiKit.h"
#include <array>
#include <memory>

/** COMP/EQ tab: the unit's single compressor plus its two-band shelving EQ.

    The EQ graph uses only documented values (shelf corners 200/400 Hz and 2/4/8 kHz, +-15 dB), with a
    standard shelf shape. The compressor curve is a SCHEMATIC: Roland publishes the ranges of
    Threshold (0-127) and Ratio (0-19) but not what each step means in dB or in ratio, so the curve
    shows how the two settings interact without putting numbers on the axes. */

/** Input/output curve with a draggable knee (threshold) and end point (ratio). Shape only. */
class CompSchematic : public ui::ParamControl
{
public:
    explicit CompSchematic (MC909EditorProcessor&);

    void paint (juce::Graphics&) override;
    void refresh() override { repaint(); }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = none; }

    const mc909::ParamDef* teachDef (juce::Point<int> p) const override
    {
        return p.x > getWidth() * 3 / 4 ? &ratio : &threshold;
    }

    /** Assumed slope for ratio step 0-19: 1:1 at 0, rising smoothly to infinity at 19. */
    static double ratioFor (int step);

private:
    enum Drag { none, knee, end };

    juce::Rectangle<float> plot() const;
    float xFor (double v) const;     // v is 0..1 along the input axis
    float yFor (double v) const;
    double vForX (float x) const;
    double vForY (float y) const;
    double kneeV() const;            // threshold as 0..1
    double endV() const;             // output at full-scale input, 0..1

    const mc909::ParamDef &threshold, &ratio;
    Drag drag = none;
};

/** Frequency response of the two shelves. Drag the left dot for the low shelf, the right for the high one:
    sideways picks the corner frequency (it snaps to the unit's choices), up and down sets the gain. */
class CompEqGraph : public ui::ParamControl
{
public:
    explicit CompEqGraph (MC909EditorProcessor&);

    void paint (juce::Graphics&) override;
    void refresh() override { repaint(); }

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { drag = -1; repaint(); }

    const mc909::ParamDef* teachDef (juce::Point<int> p) const override
    {
        return p.x < getWidth() / 2 ? &lowGain : &highGain;
    }

    /** Called ~30 times a second while the tab is showing: pulls the spectrum and repaints. */
    void tick();

    /** Level in dB of a shelf at `hz` (RBJ cookbook shelf, slope 1). */
    static double shelfDb (double hz, double cornerHz, double gainDb, bool lowShelf);

private:
    void paintSpectrum (juce::Graphics&, juce::Rectangle<float> plotArea) const;
    std::array<float, AudioAnalyser::kBins> shownSpectrum {};
    juce::Rectangle<float> plot() const;
    float xForHz (double hz) const;
    double hzForX (float x) const;
    float yForDb (double db) const;
    double dbForY (float y) const;
    double cornerHz (int which) const;
    double gainDb (int which) const;
    double totalDb (double hz) const;               // both shelves together
    juce::Point<float> handlePoint (int which) const; // on the curve, at the shelf's corner frequency
    int nearestHandle (juce::Point<float>) const;
    void apply (int which, juce::Point<float>);

    const mc909::ParamDef &lowFreq, &lowGain, &highFreq, &highGain;
    int hover = -1, drag = -1;
};

/** Left / right peak level of the audio that reaches the plugin, with peak hold. This is the MC-909's
    return AFTER its own compressor and for the whole mix, the same feed the MASTER tab shows. */
class CompLevelMeter : public juce::Component
{
public:
    explicit CompLevelMeter (MC909EditorProcessor&);
    void tick();
    void paint (juce::Graphics&) override;

private:
    MC909EditorProcessor& proc;
    float peakDb[2] { AudioAnalyser::kFloorDb, AudioAnalyser::kFloorDb };
    float holdDb[2] { AudioAnalyser::kFloorDb, AudioAnalyser::kFloorDb };
    int   holdTicks[2] {};
    bool  haveSignal = false;
};

class CompEqPage : public ui::PageBase, private juce::Timer
{
public:
    explicit CompEqPage (MC909EditorProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    std::unique_ptr<CompLevelMeter> meter;
    ui::ParamToggle* onOff = nullptr;
    CompSchematic*   curve = nullptr;
    CompEqGraph*     eq = nullptr;
    ui::ParamKnob *attack = nullptr, *release = nullptr, *thresholdKnob = nullptr, *ratioKnob = nullptr, *gain = nullptr;
    ui::ParamChoice *lowFreq = nullptr, *highFreq = nullptr, *output = nullptr;
    ui::ParamKnob *lowGain = nullptr, *highGain = nullptr, *level = nullptr, *revSend = nullptr;
    juce::Rectangle<int> compNote, outNote;
};
