#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UiKit.h"
#include "Pages.h"
//==============================================================================
/** One row: name on the left, control on the right. Continuous parameters get a
    slider, enumerated ones a combo box, driven entirely by the address map. */
class ParamRow : public juce::Component,
                 private juce::Slider::Listener,
                 private juce::ComboBox::Listener
{
public:
    ParamRow (MC909EditorProcessor&, const mc909::ParamDef&);

    void resized() override;
    void refresh();
    const mc909::ParamDef& definition() const { return def; }

private:
    void sliderValueChanged (juce::Slider*) override;
    void comboBoxChanged (juce::ComboBox*) override;

    MC909EditorProcessor& proc;
    const mc909::ParamDef& def;

    juce::Label label;
    juce::Slider slider;
    juce::ComboBox combo;
    bool updating = false;
};

//==============================================================================
class ParamPanel : public juce::Component
{
public:
    ParamPanel (MC909EditorProcessor&, mc909::Block);

    void resized() override;
    void refresh();

private:
    juce::Viewport viewport;
    juce::Component holder;
    juce::OwnedArray<ParamRow> rows;
};

//==============================================================================
/** Strip of the 8 macro knobs: shows what each is assigned to and lets you arm one for teaching. */
class MacroBar : public juce::Component, private juce::Slider::Listener
{
public:
    MacroBar (MC909EditorProcessor&, std::function<void()> onTeachChanged);

    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

    bool isTeaching() const { return teachButton.getToggleState(); }
    int  armedSlot() const  { return armed; }
    void stopTeaching();
    /** Called by the overlay with whatever the user clicked. */
    void assignArmed (const mc909::ParamDef&);

private:
    void sliderValueChanged (juce::Slider*) override {}
    void setArmed (int);

    MC909EditorProcessor& proc;
    std::function<void()> changed;
    juce::TextButton teachButton { "Teach knobs" }, clearButton { "Clear all" };
    juce::Label hint;
    struct Cell : public juce::Component
    {
        MacroBar* owner = nullptr; int slot = 0;
        juce::Slider knob;
        std::unique_ptr<juce::SliderParameterAttachment> attach;
        void resized() override { knob.setBounds (getLocalBounds().removeFromRight (34).reduced (3)); }
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
    };
    Cell cells[8];
    int armed = 0;
};

/** Transparent layer over the tabs while teaching: a click picks the control underneath instead of editing it. */
class TeachOverlay : public juce::Component
{
public:
    TeachOverlay (std::function<const mc909::ParamDef*(juce::Point<int>)> pick,
                  std::function<void (const mc909::ParamDef&)> assign)
        : pickFn (std::move (pick)), assignFn (std::move (assign)) {}
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = nullptr; repaint(); }
private:
    std::function<const mc909::ParamDef*(juce::Point<int>)> pickFn;
    std::function<void (const mc909::ParamDef&)> assignFn;
    const mc909::ParamDef* hover = nullptr;
    juce::Point<int> pos;
};

//==============================================================================
class MC909EditorComponent : public juce::AudioProcessorEditor,
                             private MC909EditorProcessor::EditListener,
                             private juce::MidiKeyboardState::Listener,
                             private juce::Timer
{
public:
    explicit MC909EditorComponent (MC909EditorProcessor&);
    ~MC909EditorComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void modelChanged() override;
    void deviceDetected (const juce::String& inputId) override;
    void timerCallback() override;

    void handleNoteOn  (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNoteNumber, float velocity) override;

    void refreshDeviceLists();
    void refreshAll();
    void updateAccent();

   #ifdef MC909_UI_DEMO
    void seedDemo();   // screenshot builds only: fills the local mirror with plausible values
   #endif

    // Declared first so it is destroyed last, after every component using it.
    ui::SynthLookAndFeel lookAndFeel;

    MC909EditorProcessor& proc;

    juce::ComboBox midiOutBox, midiInBox, deviceIdBox, partBox;
    juce::TextButton toneButtons[4];
    juce::TextButton detectButton { "Detect" },
                     getButton    { "Get from MC-909" },
                     sendButton   { "Send to MC-909" },
                     probeButton  { "Probe" };
    juce::ToggleButton toneMaskButtons[4];
    juce::Label automateLabel { {}, "AUTOMATE" };
    juce::Label patchNameLabel { {}, "Patch" };
    juce::TextEditor patchNameEditor;
    juce::Label statusLabel;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::OwnedArray<ui::PageBase> pages;
    juce::OwnedArray<ParamPanel> panels;

    std::unique_ptr<MacroBar> macroBar;
    std::unique_ptr<TeachOverlay> teachOverlay;
    const mc909::ParamDef* pickAt (juce::Point<int> inEditor);
    void teachModeChanged();

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    bool needsRefresh = false;
    bool deviceSeen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MC909EditorComponent)
};
