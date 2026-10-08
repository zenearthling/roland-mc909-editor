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
                     sendButton   { "Send to MC-909" };
    juce::ToggleButton toneMaskButtons[4];
    juce::Label automateLabel { {}, "AUTOMATE" };
    juce::Label patchNameLabel { {}, "Patch" };
    juce::TextEditor patchNameEditor;
    juce::Label statusLabel;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::OwnedArray<ui::PageBase> pages;
    juce::OwnedArray<ParamPanel> panels;

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    bool needsRefresh = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MC909EditorComponent)
};
