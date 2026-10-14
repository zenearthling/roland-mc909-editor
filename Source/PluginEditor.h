#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "UiKit.h"
#include "Pages.h"
#include "SearchOverlay.h"
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
    bool isHelping() const  { return helpButton.getToggleState(); }
    void stopHelping();
    void setHelp (bool on) { helpButton.setToggleState (on, juce::dontSendNotification); if (onHelp) onHelp(); }
    std::function<void()> onHelp;
    int  armedSlot() const  { return armed; }
    void stopTeaching();
    /** Called by the overlay with whatever the user clicked. */
    void assignArmed (const mc909::ParamDef&);

private:
    void sliderValueChanged (juce::Slider*) override {}
    void setArmed (int);

    MC909EditorProcessor& proc;
    std::function<void()> changed;
    juce::TextButton teachButton { "Teach knobs" }, clearButton { "Clear all" }, helpButton { "Help" };
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

/** Help mode: hovering a control shows a tile describing it. Clicks are swallowed so nothing is edited. */
class HelpOverlay : public juce::Component
{
public:
    using Lookup = std::function<bool (juce::Point<int>, juce::String& title, juce::String& range, juce::String& body)>;

    explicit HelpOverlay (Lookup l) : lookup (std::move (l)) {}
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent& e) override { pos = e.getPosition(); found = lookup (pos, title, range, body); repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override { mouseMove (e); }
    void mouseExit (const juce::MouseEvent&) override { found = false; repaint(); }
    void demoHover (const juce::String& t, const juce::String& r, const juce::String& b, juce::Point<int> p)
    {
        title = t; range = r; body = b; pos = p; found = t.isNotEmpty(); repaint();
    }
private:
    Lookup lookup;
    bool found = false;
    juce::String title, range, body;
    juce::Point<int> pos;
};

//==============================================================================
/** On-screen keyboard that numbers the 16 keys the MC-909's pads play (Rhythm Tone Key #59..#74),
    so "pad 5" on the unit and the key you click here are obviously the same thing. */
class PadKeyboard : public juce::MidiKeyboardComponent
{
public:
    static constexpr int kFirstPadNote = 59, kNumPads = 16;

    PadKeyboard (juce::MidiKeyboardState& state, Orientation o) : juce::MidiKeyboardComponent (state, o) {}

    static bool isPadNote (int note) { return note >= kFirstPadNote && note < kFirstPadNote + kNumPads; }

protected:
    void drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour line, juce::Colour text) override
    {
        // A "C4" label and a pad number in the same spot would just fight, so the number wins.
        juce::MidiKeyboardComponent::drawWhiteNote (note, g, area, isDown, isOver, line,
                                                    isPadNote (note) ? juce::Colours::transparentBlack : text);
        if (isPadNote (note))
            badge (g, area, note, 5.0f);
    }

    void drawBlackNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                        juce::Colour fill) override
    {
        juce::MidiKeyboardComponent::drawBlackNote (note, g, area, isDown, isOver, fill);
        if (isPadNote (note))
            badge (g, area, note, 4.0f);
    }

private:
    static void badge (juce::Graphics& g, juce::Rectangle<float> keyArea, int note, float bottomGap)
    {
        const float d = juce::jmin (17.0f, keyArea.getWidth() - 3.0f);
        if (d < 8.0f)
            return;

        const auto r = juce::Rectangle<float> (d, d).withCentre ({ keyArea.getCentreX(), keyArea.getBottom() - bottomGap - d * 0.5f });
        g.setColour (juce::Colour (0xffff8a3d));
        g.fillEllipse (r);
        g.setColour (juce::Colour (0xff17191e));
        g.setFont (juce::Font (juce::FontOptions (d * 0.66f, juce::Font::bold)));
        g.drawText (juce::String (note - kFirstPadNote + 1), r.toNearestInt(), juce::Justification::centred, false);
    }
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
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int kBaseW = 1120, kBaseH = 824;

private:
    void paintContent (juce::Graphics&);
    void zoomBy (float factor);
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

    /** Holds every visible child, laid out at kBaseW x kBaseH and scaled to the window. Declared before the
        children so it is destroyed after them. */
    struct Content : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    } content;
    juce::TextButton zoomOutButton { "A-" }, zoomInButton { "A+" };

    juce::ComboBox midiOutBox, midiInBox, deviceIdBox, partBox;
    juce::TextButton toneButtons[4];
    juce::TextButton searchButton { "Search..." };
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

    juce::Label channelLabel;

    // What the search last put on each part ("Preset A 001  Trance Chord"), shown right away
    // rather than waiting for the unit to answer a Get. The hardware's own name is in the Patch box.
    juce::Label loadedLabel;
    juce::String loadedText[16];
    void setLoaded (int part, const juce::String& text);
    std::unique_ptr<MacroBar> macroBar;
    std::unique_ptr<TeachOverlay> teachOverlay;
    std::unique_ptr<HelpOverlay> helpOverlay;
    const mc909::ParamDef* pickAt (juce::Point<int> inEditor, juce::Component* overlay = nullptr);
    bool helpAt (juce::Point<int> inOverlay, juce::String& title, juce::String& range, juce::String& body);
    void teachModeChanged();
    void helpModeChanged();

    // Browser over every waveform / patch / kit / pattern in the MC-909 patch list.
    std::unique_ptr<SearchOverlay> searchOverlay;
    void openSearch();
    bool rhythmTabActive() const;
    juce::String searchActionText (const mc909search::Entry&) const;
    juce::String performSearchAction (const mc909search::Entry&, bool audition);
    void auditionSoon (int note, int delayMs);
    void flashStatus (const juce::String&);
    juce::String statusFlash;
    juce::uint32 statusFlashUntil = 0;

    juce::MidiKeyboardState keyboardState;
    PadKeyboard keyboard { keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard };

    bool needsRefresh = false;
    bool deviceSeen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MC909EditorComponent)
};
