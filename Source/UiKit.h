#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <initializer_list>
#include <utility>

/** Visual building blocks for the editor: a dark look-and-feel, tone colours,
    and parameter-bound controls (knobs, toggles, number boxes) plus the three
    live displays (envelope editor, filter response, LFO schematic).

    Everything binds to the same mc909::ParamDef table the old list UI used, so
    the SysEx path is untouched: a control only ever calls
    MC909EditorProcessor::setParameterValue / getParameterValue. */
namespace ui
{

enum ColourIds
{
    accentColourId = 0x2f000001   ///< current tone colour; looked up through parents
};

namespace col
{
    const juce::Colour bg       { 0xff121418 };
    const juce::Colour card     { 0xff1b1e24 };
    const juce::Colour cardEdge { 0xff2a2f38 };
    const juce::Colour well     { 0xff0e1014 };
    const juce::Colour text     { 0xffe8ebf0 };
    const juce::Colour textDim  { 0xff8c93a0 };
    const juce::Colour knobBody { 0xff2b303a };
    const juce::Colour track    { 0xff343a46 };
}

/** One accent per tone, shared by the tone buttons, knobs and displays. */
juce::Colour toneColour (int tone /* 0-3 */);

juce::Font font (float height, bool bold = false);

/** Look up a parameter by its stable id ("tone.tvf_cutoff"). Never returns
    null: a typo asserts in debug and falls back to the first entry in release. */
const mc909::ParamDef& findParam (juce::StringRef id);

//==============================================================================
class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by,
                       int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool isMouseOver, bool isMouseDown) override;
    int  getTabButtonBestWidth (juce::TabBarButton&, int tabDepth) override;
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int w, int h) override;
};

//==============================================================================
/** Anything bound to the editing model: re-reads its value on refresh(). */
class ParamControl : public juce::Component
{
public:
    explicit ParamControl (MC909EditorProcessor& p) : proc (p) {}
    virtual void refresh() = 0;

protected:
    MC909EditorProcessor& proc;
};

/** Rotary knob with a caption underneath and the value readout below the dial. */
class ParamKnob : public ParamControl, private juce::Slider::Listener
{
public:
    ParamKnob (MC909EditorProcessor&, const juce::String& id, const juce::String& caption = {});
    void resized() override;
    void refresh() override;

    static constexpr int cellWidth = 60, cellHeight = 86;

private:
    void sliderValueChanged (juce::Slider*) override;

    const mc909::ParamDef& def;
    juce::Slider slider;
    juce::Label  name;
    bool updating = false;
};

/** Caption over a combo box, for enumerated parameters. `compact` drops the
    caption so the combo fills its bounds (used on top of displays and in grids). */
class ParamChoice : public ParamControl, private juce::ComboBox::Listener
{
public:
    ParamChoice (MC909EditorProcessor&, const juce::String& id,
                 const juce::String& caption = {}, bool compact = false);
    void resized() override;
    void refresh() override;

private:
    void comboBoxChanged (juce::ComboBox*) override;

    const mc909::ParamDef& def;
    juce::Label    name;
    juce::ComboBox combo;
    bool compactMode = false;
    bool updating = false;
};

/** OFF/ON pill for two-state parameters. */
class ParamToggle : public ParamControl
{
public:
    ParamToggle (MC909EditorProcessor&, const juce::String& id, const juce::String& caption = {});
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh() override;

private:
    const mc909::ParamDef& def;
    juce::String caption;
    bool on = false;
};

/** Drag-to-change integer readout, used for the wave numbers (0-16384). */
class ParamNumber : public ParamControl
{
public:
    ParamNumber (MC909EditorProcessor&, const juce::String& id, const juce::String& caption = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void refresh() override;

private:
    void commit (int raw);

    const mc909::ParamDef& def;
    juce::String caption;
    juce::TextEditor editor;
    int value = 0, dragStart = 0;
};

//==============================================================================
/** Draggable envelope: x is time (each segment is a quarter of the plot at max),
    y is level. Pitch/Filter use 4 times + 5 levels, Amp uses 4 times + 3 levels
    with fixed zero endpoints, as in the address map. Times are the unit-less
    0-127 values the hardware stores, so the picture is relative, not seconds. */
class EnvelopeEditor : public ParamControl
{
public:
    enum class Kind { pitch, filter, amp };

    /** `root` is the id prefix of the parameter family: "tone." for patch tones, "rn." for rhythm pads. */
    EnvelopeEditor (MC909EditorProcessor&, Kind, const juce::String& root = "tone.");

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void refresh() override { repaint(); }

private:
    juce::Rectangle<float> plot() const;
    float levelNorm (int node) const;
    juce::Point<float> nodePos (int node) const;
    int  hitTest (juce::Point<float>) const;
    bool nodeMovable (int node) const;
    void setIfChanged (const mc909::ParamDef&, int raw);
    juce::String describe (int node) const;

    Kind kind;
    const mc909::ParamDef* timeDef[4] {};
    const mc909::ParamDef* levelDef[5] {};
    int hoverNode = -1, dragNode = -1;
};

/** Magnitude response of the selected filter type at the current cutoff and
    resonance. A schematic of the shape, not a measurement of the hardware. */
class FilterCurve : public ParamControl
{
public:
    explicit FilterCurve (MC909EditorProcessor&, const juce::String& root = "tone.");
    void paint (juce::Graphics&) override;
    void refresh() override { repaint(); }

private:
    const mc909::ParamDef &typeDef, &cutoffDef, &resoDef;
};

/** Delay, fade-in and overall depth of one LFO drawn as a wave. Schematic only:
    the rate axis is relative and the waveform is always drawn as a sine. */
class LfoScope : public ParamControl
{
public:
    LfoScope (MC909EditorProcessor&, int lfoIndex /* 1 or 2 */);
    void paint (juce::Graphics&) override;
    void refresh() override { repaint(); }

private:
    int raw (const char* suffix) const;

    juce::String prefix;
};

//==============================================================================
/** Base for a page of controls laid out on painted cards. */
class PageBase : public juce::Component
{
public:
    explicit PageBase (MC909EditorProcessor& p) : proc (p) {}

    void refresh();
    void paint (juce::Graphics&) override;

protected:
    struct Card { juce::Rectangle<int> area; juce::String title; };

    template <class T, class... Args>
    T* add (Args&&... args)
    {
        auto* c = new T (proc, std::forward<Args> (args)...);
        controls.add (c);
        addAndMakeVisible (c);
        return c;
    }

    /** Lay knobs out left-to-right, top-to-bottom inside an area. */
    static void placeKnobs (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int columns);

    static juce::Rectangle<int> contentOf (const Card& c);

    MC909EditorProcessor& proc;
    std::vector<Card> cards;

private:
    juce::OwnedArray<ParamControl> controls;
};

} // namespace ui
