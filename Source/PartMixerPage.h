#pragma once
#include "UiKit.h"
#include <functional>

/** PART tab: all 16 parts as a mixer.

    Each strip shows the part's patch, its Receive switch, Output Select, Pan, Level and the Dry and
    Reverb sends. They are the Part Info settings of the MC-909 (Part Info Part 1-16), so every
    move goes out as a normal parameter change, and the values are read back from the unit.

    There are no level meters: the unit does not report per-part audio over MIDI. */

class PartMixerBoard : public ui::ParamControl
{
public:
    explicit PartMixerBoard (MC909EditorProcessor&);

    void paint (juce::Graphics&) override;
    void refresh() override { repaint(); }

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverPart = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragWhat = none; }
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    const mc909::ParamDef* teachDef (juce::Point<int>) const override;

    /** Called when a strip other than the selected part is clicked. */
    std::function<void (int part)> onPartPicked;

    enum Area { none, header, rx, output, pan, level, dry, rev };

    struct StripRects
    {
        juce::Rectangle<float> strip, header, rx, output, pan, level, dry, rev;
    };

    StripRects rects (int part) const;
    struct Hit { int part = -1; Area area = none; };
    Hit hitTest (juce::Point<float>) const;

    /** What the strip's header shows: the selected part's real patch name, otherwise looked up from
        the bank and program in the patch list. */
    juce::String partTitle (int part) const;
    static juce::String lookUpName (int msb, int lsb, int program);

    void setOutput (int part, int value);   ///< also what the pop-up menu calls

private:
    void applyDrag (juce::Point<float>);
    void choosePart (int part);

    const mc909::ParamDef &rxDef, &levelDef, &panDef, &dryDef, &revDef, &outDef;
    Area dragWhat = none;
    int dragPart = -1, hoverPart = -1;
};

class PartMixerPage : public ui::PageBase
{
public:
    explicit PartMixerPage (MC909EditorProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void visibilityChanged() override;

    PartMixerBoard& getBoard() { return *board; }

private:
    PartMixerBoard* board = nullptr;
    juce::TextButton readButton { "Read all parts" };
    ui::ParamKnob *coarse = nullptr, *fine = nullptr;
    ui::ParamToggle* autoSync = nullptr;
    ui::ParamNumber *bankMsb = nullptr, *bankLsb = nullptr, *program = nullptr;
    juce::Rectangle<int> noteArea, detailCard;
};
