#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "SearchIndex.h"

/** Ableton-style browser over everything in the MC-909 patch list: waveforms,
    patches, rhythm kits, patterns and the pattern / RPS sets.

    Flat grey panel, a search field that filters as you type, a category column
    with live counts, an orange selection bar, and an info pane for the selected
    item. Keyboard first: type, Up/Down, Enter. The overlay knows nothing about
    the hardware; the editor tells it what Enter does through `Host`. */
class SearchOverlay : public juce::Component,
                      private juce::TextEditor::Listener,
                      private juce::ListBoxModel,
                      private juce::KeyListener,
                      private juce::Timer
{
public:
    struct Host
    {
        /** What Enter would do with this item right now ("Load into pad C#4, tone 1"). Empty: nothing. */
        std::function<juce::String (const mc909search::Entry&)> describeAction;

        /** Do it. `audition` asks for a test note afterwards. Returns a short status line. */
        std::function<juce::String (const mc909search::Entry&, bool audition)> perform;

        /** The overlay has hidden itself; give keyboard focus back. */
        std::function<void()> closed;
    };

    explicit SearchOverlay (Host);
    ~SearchOverlay() override;

    void open();
    void close();

    /** Open straight onto the waveform list with `currentWave` selected (0 = none). While it is
        open, moving the selection loads each wave onto the selected pad / tone and plays it.
        Enter keeps the wave; Esc puts the original one back. */
    void openOnWaveforms (int currentWave);
    bool isOpen() const { return isVisible(); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    // juce::KeyListener: the search field would otherwise swallow Up / Down / Enter.
    bool keyPressed (const juce::KeyPress& k, juce::Component*) override { return handleKey (k); }
    bool handleKey (const juce::KeyPress&);

    void textEditorTextChanged (juce::TextEditor&) override { runSearch(); }

    int  getNumRows() override { return (int) shown.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int) override;

    void timerCallback() override;

    void runSearch();
    void applyCategory (int category);
    void moveSelection (int delta);
    void activate (bool keepOpen);
    const mc909search::Entry* selectedEntry() const;
    juce::Rectangle<int> categoryRow (int category) const;

    void paintSidebar (juce::Graphics&);
    void paintInfo (juce::Graphics&);
    void paintFooter (juce::Graphics&);

    Host host;
    const mc909search::Searcher& searcher;

    juce::TextEditor searchField;
    juce::ListBox    results { {}, this };

    juce::Rectangle<int> panel, header, sidebar, listArea, info, footer, clearButton, auditionBox;

    std::string query;
    std::vector<mc909search::Hit> shown;        // current query, current category
    int counts[mc909search::kNumKinds] {};      // current query, every category
    int totalCount = 0;
    int category = 0;                           // 0 = All, 1.. = Kind + 1
    int hoverRow = -1, hoverCategory = -1;
    bool audition = true;

    // openOnWaveforms(): load-as-you-browse
    bool autoLoad = false, committed = false, autoChanged = false;
    int  originalWave = 0;
    int  loadToken = 0;

    juce::String flash;                         // result of the last Shift+Enter

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SearchOverlay)
};
