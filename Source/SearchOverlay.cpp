#include "SearchOverlay.h"
#include "UiKit.h"

using namespace mc909search;

//==============================================================================
// A flat grey palette in the spirit of Live's browser: no gradients, hairline
// dividers, almost square corners, one orange accent for the selection.
namespace pal
{
    const juce::Colour scrim      { 0xa8000000 };
    const juce::Colour panel      { 0xff2d2d2d };
    const juce::Colour panelEdge  { 0xff0e0e0e };
    const juce::Colour sidebar    { 0xff262626 };
    const juce::Colour field      { 0xff1b1b1b };
    const juce::Colour fieldEdge  { 0xff0b0b0b };
    const juce::Colour divider    { 0xff1a1a1a };
    const juce::Colour hover      { 0xff383838 };
    const juce::Colour text       { 0xffd4d4d4 };
    const juce::Colour textDim    { 0xff8f8f8f };
    const juce::Colour textFaint  { 0xff626262 };
    const juce::Colour accent     { 0xffffad33 };
    const juce::Colour onAccent   { 0xff1a1a1a };

    /** One dot colour per kind, like the colour tags in Live's sidebar. */
    juce::Colour dot (Kind k)
    {
        switch (k)
        {
            case Kind::waveform:   return juce::Colour (0xff4fb4ff);
            case Kind::patch:      return juce::Colour (0xffffa23a);
            case Kind::rhythmKit:  return juce::Colour (0xffff6060);
            case Kind::pattern:    return juce::Colour (0xff6fd16f);
            case Kind::patternSet: return juce::Colour (0xffb98cff);
            case Kind::rpsPattern: return juce::Colour (0xffffd84d);
            case Kind::rpsSet:     return juce::Colour (0xff9aa0a8);
        }
        return textDim;
    }
}

namespace
{
    constexpr int kRowH = 26, kCatH = 28, kHeaderH = 54, kFooterH = 42;
    constexpr int kSidebarW = 176, kInfoW = 262;

    const Searcher& sharedSearcher()
    {
        static const Searcher s;      // ~1.5 ms to build; lives for the process
        return s;
    }

    juce::String s2j (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }

    /** Draws `text` with some character ranges picked out (the part you typed). */
    void drawHighlighted (juce::Graphics& g, const juce::String& text, const std::vector<Range>& ranges,
                          juce::Rectangle<int> area, const juce::Font& font, juce::Colour base, juce::Colour hit)
    {
        juce::AttributedString as;
        as.setJustification (juce::Justification::centredLeft);
        as.setWordWrap (juce::AttributedString::none);

        int pos = 0;
        auto addRun = [&] (int from, int to, bool highlighted)
        {
            if (to <= from) return;
            as.append (text.substring (from, to), highlighted ? font.boldened() : font, highlighted ? hit : base);
        };

        for (const auto& r : ranges)
        {
            addRun (pos, r.start, false);
            addRun (r.start, r.start + r.length, true);
            pos = r.start + r.length;
        }
        addRun (pos, text.length(), false);

        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (area);
        as.draw (g, area.toFloat());
    }

    void drawMagnifier (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        const float d = r.getHeight() * 0.62f;
        g.drawEllipse (r.getX() + 1.0f, r.getY() + 1.0f, d, d, 1.6f);
        juce::Line<float> handle (r.getX() + 1.0f + d * 0.88f, r.getY() + 1.0f + d * 0.88f,
                                  r.getRight() - 1.0f, r.getBottom() - 1.0f);
        g.drawLine (handle, 2.0f);
    }
}

//==============================================================================
SearchOverlay::SearchOverlay (Host h)
    : host (std::move (h)), searcher (sharedSearcher())
{
    setWantsKeyboardFocus (true);
    setInterceptsMouseClicks (true, true);

    searchField.setMultiLine (false);
    searchField.setReturnKeyStartsNewLine (false);
    searchField.setScrollbarsShown (false);
    searchField.setPopupMenuEnabled (false);
    searchField.setFont (ui::font (14.0f));
    searchField.setTextToShowWhenEmpty ("Search waveforms, patches, kits, patterns...", pal::textFaint);
    searchField.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    searchField.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    searchField.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    searchField.setColour (juce::TextEditor::textColourId, pal::text);
    searchField.setColour (juce::TextEditor::highlightColourId, pal::accent.withAlpha (0.45f));
    searchField.setColour (juce::TextEditor::highlightedTextColourId, pal::text);
    searchField.setColour (juce::CaretComponent::caretColourId, pal::accent);
    searchField.setIndents (0, 0);
    searchField.addListener (this);
    searchField.addKeyListener (this);
    addAndMakeVisible (searchField);

    results.setRowHeight (kRowH);
    results.setOutlineThickness (0);
    results.setWantsKeyboardFocus (false);      // typing always goes to the search field
    results.setColour (juce::ListBox::backgroundColourId, pal::panel);
    results.setMouseMoveSelectsRows (false);
    results.addMouseListener (this, true);
    results.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, pal::textFaint);
    addAndMakeVisible (results);

    setVisible (false);
}

SearchOverlay::~SearchOverlay()
{
    searchField.removeKeyListener (this);
    results.removeMouseListener (this);
    stopTimer();
}

//==============================================================================
void SearchOverlay::open()
{
    setVisible (true);
    toFront (true);
    flash.clear();
    runSearch();
    searchField.grabKeyboardFocus();
    searchField.selectAll();             // typing replaces the last query, like Live
    repaint();
}

void SearchOverlay::close()
{
    if (! isVisible())
        return;

    stopTimer();
    setVisible (false);
    if (host.closed)
        host.closed();
}

void SearchOverlay::timerCallback()
{
    stopTimer();
    flash.clear();
    repaint();
}

//==============================================================================
void SearchOverlay::runSearch()
{
    query = searchField.getText().toStdString();
    const auto all = searcher.search (query);

    for (auto& c : counts) c = 0;
    for (const auto& h : all)
        ++counts[(int) kEntries[h.index].kind];
    totalCount = (int) all.size();

    shown.clear();
    for (const auto& h : all)
        if (category == 0 || (int) kEntries[h.index].kind == category - 1)
            shown.push_back (h);

    // With nothing to list, hide the list so paint() can say so: a child component
    // would otherwise paint straight over the message.
    results.setVisible (! shown.empty());
    results.updateContent();
    results.scrollToEnsureRowIsOnscreen (0);
    if (! shown.empty())
        results.selectRow (0);
    else
        results.deselectAllRows();

    hoverRow = -1;
    repaint();
}

void SearchOverlay::applyCategory (int c)
{
    category = juce::jlimit (0, kNumKinds, c);
    runSearch();
}

void SearchOverlay::moveSelection (int delta)
{
    if (shown.empty())
        return;

    const int row = juce::jlimit (0, (int) shown.size() - 1, results.getSelectedRow() + delta);
    results.selectRow (row);
    results.scrollToEnsureRowIsOnscreen (row);
}

const Entry* SearchOverlay::selectedEntry() const
{
    const int row = results.getSelectedRow();
    return juce::isPositiveAndBelow (row, (int) shown.size()) ? &kEntries[shown[(size_t) row].index] : nullptr;
}

void SearchOverlay::activate (bool keepOpen)
{
    const auto* e = selectedEntry();
    if (e == nullptr || ! host.perform)
        return;

    const auto message = host.perform (*e, audition);

    if (keepOpen)
    {
        flash = message;
        startTimer (3500);
        repaint();
    }
    else
        close();
}

void SearchOverlay::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
{
    results.selectRow (row);
    activate (false);
}

//==============================================================================
bool SearchOverlay::keyPressed (const juce::KeyPress& k)
{
    return handleKey (k);
}

bool SearchOverlay::handleKey (const juce::KeyPress& k)
{
    const int code = k.getKeyCode();
    const bool shift = k.getModifiers().isShiftDown();

    if (code == juce::KeyPress::escapeKey)
    {
        // First Esc clears the query (as in Live's browser), the second closes.
        if (searchField.getText().isNotEmpty())  searchField.clear();
        else                                     close();
        return true;
    }
    if (code == juce::KeyPress::downKey)     { moveSelection (1);   return true; }
    if (code == juce::KeyPress::upKey)       { moveSelection (-1);  return true; }
    if (code == juce::KeyPress::pageDownKey) { moveSelection (10);  return true; }
    if (code == juce::KeyPress::pageUpKey)   { moveSelection (-10); return true; }

    if (code == juce::KeyPress::returnKey)
    {
        activate (shift);              // Shift+Enter: load and keep browsing
        return true;
    }

    if (code == juce::KeyPress::tabKey)
    {
        applyCategory ((category + (shift ? kNumKinds : 1)) % (kNumKinds + 1));
        return true;
    }

    return false;
}

//==============================================================================
void SearchOverlay::resized()
{
    panel = getLocalBounds().withSizeKeepingCentre (juce::jmin (getWidth() - 40, 960),
                                                    juce::jmin (getHeight() - 60, 640));
    auto r = panel;
    header  = r.removeFromTop (kHeaderH);
    footer  = r.removeFromBottom (kFooterH);
    sidebar = r.removeFromLeft (kSidebarW);
    info    = r.removeFromRight (kInfoW);
    listArea = r;

    auto box = header.reduced (12, 10);
    clearButton = box.removeFromRight (28);
    searchField.setBounds (box.withTrimmedLeft (30).withTrimmedRight (4).reduced (0, 4));
    results.setBounds (listArea);

    auditionBox = footer.reduced (14, 0).removeFromRight (96).withSizeKeepingCentre (96, 22);
}

juce::Rectangle<int> SearchOverlay::categoryRow (int c) const
{
    return { sidebar.getX(), sidebar.getY() + 34 + c * kCatH, sidebar.getWidth(), kCatH };
}

//==============================================================================
void SearchOverlay::mouseMove (const juce::MouseEvent& e)
{
    const auto inResults = e.getEventRelativeTo (&results).getPosition();
    int row = -1;
    if (results.getLocalBounds().contains (inResults))
        row = results.getRowContainingPosition (inResults.x, inResults.y);

    const auto inSelf = e.getEventRelativeTo (this).getPosition();
    int cat = -1;
    for (int c = 0; c <= kNumKinds; ++c)
        if (categoryRow (c).contains (inSelf))
            cat = c;

    if (row != hoverRow || cat != hoverCategory)
    {
        hoverRow = row;
        hoverCategory = cat;
        repaint();
    }
}

void SearchOverlay::mouseExit (const juce::MouseEvent&)
{
    if (hoverRow != -1 || hoverCategory != -1)
    {
        hoverRow = hoverCategory = -1;
        repaint();
    }
}

void SearchOverlay::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getEventRelativeTo (this).getPosition();

    if (! panel.contains (p))
    {
        close();
        return;
    }

    if (clearButton.contains (p) && searchField.getText().isNotEmpty())
    {
        searchField.clear();
        searchField.grabKeyboardFocus();
        return;
    }

    for (int c = 0; c <= kNumKinds; ++c)
        if (categoryRow (c).contains (p))
        {
            applyCategory (c);
            searchField.grabKeyboardFocus();
            return;
        }

    if (auditionBox.contains (p))
    {
        audition = ! audition;
        repaint();
        return;
    }
}

//==============================================================================
void SearchOverlay::paint (juce::Graphics& g)
{
    g.fillAll (pal::scrim);

    // drop shadow, then the panel
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (panel.translated (0, 4).expanded (2));
    g.setColour (pal::panel);
    g.fillRect (panel);

    // --- search header ------------------------------------------------------
    {
        g.setColour (pal::sidebar);
        g.fillRect (header);
        g.setColour (pal::divider);
        g.fillRect (header.getX(), header.getBottom() - 1, header.getWidth(), 1);

        auto box = header.reduced (12, 10);
        g.setColour (pal::field);
        g.fillRect (box);
        g.setColour (searchField.hasKeyboardFocus (true) ? pal::accent.withAlpha (0.8f) : pal::fieldEdge);
        g.drawRect (box, 1);

        drawMagnifier (g, juce::Rectangle<float> ((float) box.getX() + 9.0f, (float) box.getCentreY() - 8.0f, 16.0f, 16.0f),
                       pal::textDim);

        if (searchField.getText().isNotEmpty())
        {
            g.setColour (clearButton.contains (getMouseXYRelative()) ? pal::text : pal::textDim);
            const auto c = clearButton.toFloat().getCentre();
            g.drawLine (c.x - 4, c.y - 4, c.x + 4, c.y + 4, 1.6f);
            g.drawLine (c.x - 4, c.y + 4, c.x + 4, c.y - 4, 1.6f);
        }
    }

    paintSidebar (g);
    paintInfo (g);
    paintFooter (g);

    // dividers between the three columns
    g.setColour (pal::divider);
    g.fillRect (sidebar.getRight() - 1, sidebar.getY(), 1, sidebar.getHeight());
    g.fillRect (info.getX(), info.getY(), 1, info.getHeight());

    if (shown.empty())
    {
        g.setColour (pal::textDim);
        g.setFont (ui::font (12.0f));
        g.drawFittedText (query.empty() ? juce::String ("Nothing here") : "No results for \"" + s2j (query) + "\"",
                          listArea.reduced (20), juce::Justification::centred, 2);
    }

    g.setColour (pal::panelEdge);
    g.drawRect (panel, 1);
}

void SearchOverlay::paintSidebar (juce::Graphics& g)
{
    g.setColour (pal::sidebar);
    g.fillRect (sidebar);

    g.setColour (pal::textFaint);
    g.setFont (ui::font (9.5f, true).withExtraKerningFactor (0.12f));
    g.drawText ("BROWSE", sidebar.getX() + 16, sidebar.getY() + 8, sidebar.getWidth() - 32, 20, juce::Justification::centredLeft);

    for (int c = 0; c <= kNumKinds; ++c)
    {
        auto row = categoryRow (c);
        const bool selected = (c == category);
        const int n = (c == 0) ? totalCount : counts[c - 1];

        if (selected || c == hoverCategory)
        {
            g.setColour (selected ? pal::hover.brighter (0.05f) : pal::hover.withAlpha (0.55f));
            g.fillRect (row);
        }
        if (selected)
        {
            g.setColour (pal::accent);
            g.fillRect (row.removeFromLeft (3));
        }

        auto content = categoryRow (c).reduced (16, 0);
        if (c > 0)
        {
            g.setColour (pal::dot (Kind (c - 1)).withAlpha (n > 0 ? 1.0f : 0.35f));
            g.fillEllipse (content.getX() + 0.0f, (float) content.getCentreY() - 4.0f, 8.0f, 8.0f);
        }
        content.removeFromLeft (18);

        const auto textCol = n == 0 ? pal::textFaint : (selected ? pal::text : pal::textDim.brighter (0.25f));
        g.setColour (textCol);
        g.setFont (ui::font (11.0f, selected));
        auto numArea = content.removeFromRight (44);
        g.drawText (c == 0 ? "All" : kindLabelPlural (Kind (c - 1)), content, juce::Justification::centredLeft, true);
        g.setColour (pal::textFaint);
        g.setFont (ui::font (10.0f));
        g.drawText (juce::String (n), numArea, juce::Justification::centredRight);
    }

    g.setColour (pal::textFaint);
    g.setFont (ui::font (9.5f));
    g.drawFittedText ("Tab switches category", sidebar.withTrimmedTop (sidebar.getHeight() - 36).reduced (16, 6),
                      juce::Justification::bottomLeft, 1);
}

void SearchOverlay::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) shown.size()))
        return;

    const auto& hit = shown[(size_t) row];
    const auto& e = kEntries[hit.index];
    const juce::Rectangle<int> area (0, 0, w, h);

    if (selected)
    {
        g.setColour (pal::accent);
        g.fillRect (area);
    }
    else if (row == hoverRow)
    {
        g.setColour (pal::hover);
        g.fillRect (area);
    }

    const auto main = selected ? pal::onAccent : pal::text;
    const auto dim  = selected ? pal::onAccent.withAlpha (0.65f) : pal::textDim;
    const auto hi   = selected ? pal::onAccent : pal::accent;

    g.setColour (selected ? pal::onAccent.withAlpha (0.7f) : pal::dot (e.kind));
    g.fillEllipse (14.0f, (float) h * 0.5f - 3.5f, 7.0f, 7.0f);

    // right-hand column: where it lives
    auto content = area.withTrimmedLeft (32).withTrimmedRight (12);
    g.setColour (dim);
    g.setFont (ui::font (10.0f));
    auto where = content.removeFromRight (juce::jmin (150, content.getWidth() / 3));
    g.drawText (e.where, where, juce::Justification::centredRight, true);
    content.removeFromRight (8);

    // name, with what you typed picked out
    const auto font = ui::font (11.5f);
    const juce::String name (juce::CharPointer_UTF8 (e.name));
    const int nameW = juce::jmin (content.getWidth(), juce::jmax (150, content.getWidth() * 62 / 100));
    drawHighlighted (g, name, searcher.highlight (hit.index, query), content.removeFromLeft (nameW), font, main, hi);

    // why a kit / pattern matched when the name didn't
    if (hit.extraSegment >= 0 && content.getWidth() > 40)
    {
        const auto seg = searcher.extraSegment (hit.index, hit.extraSegment);
        g.setColour (dim);
        g.setFont (ui::font (10.0f));
        g.drawText ("in", content.removeFromLeft (14), juce::Justification::centredLeft);
        drawHighlighted (g, s2j (seg), searcher.highlightExtra (hit.index, hit.extraSegment, query),
                         content, ui::font (10.5f), dim, hi);
    }
}

void SearchOverlay::paintInfo (juce::Graphics& g)
{
    g.setColour (pal::sidebar);
    g.fillRect (info);

    const auto* e = selectedEntry();
    if (e == nullptr)
        return;

    auto r = info.reduced (16, 14);

    g.setColour (pal::dot (e->kind));
    g.fillEllipse ((float) r.getX(), (float) r.getY() + 5.0f, 8.0f, 8.0f);
    g.setColour (pal::textDim);
    g.setFont (ui::font (10.0f, true).withExtraKerningFactor (0.1f));
    g.drawText (juce::String (kindLabel (e->kind)).toUpperCase(), r.removeFromTop (18).withTrimmedLeft (16),
                juce::Justification::centredLeft);
    r.removeFromTop (4);

    g.setColour (pal::text);
    g.setFont (ui::font (15.0f, true));
    const juce::String name (juce::CharPointer_UTF8 (e->name));
    g.drawFittedText (name, r.removeFromTop (44), juce::Justification::topLeft, 2, 0.9f);

    g.setColour (pal::textDim);
    g.setFont (ui::font (10.5f));
    g.drawText (e->where, r.removeFromTop (18), juce::Justification::centredLeft, true);
    r.removeFromTop (8);
    g.setColour (pal::divider);
    g.fillRect (r.removeFromTop (1));
    r.removeFromTop (8);

    // detail: lines of "label<TAB>value", "# " starts a heading
    const auto lines = juce::StringArray::fromLines (juce::String (juce::CharPointer_UTF8 (e->detail)));
    for (const auto& line : lines)
    {
        if (r.getHeight() < 16)
            break;

        if (line.startsWith ("# "))
        {
            r.removeFromTop (6);
            g.setColour (pal::textFaint);
            g.setFont (ui::font (9.5f, true).withExtraKerningFactor (0.06f));
            g.drawText (line.substring (2).toUpperCase(), r.removeFromTop (18), juce::Justification::centredLeft, true);
            continue;
        }

        auto row = r.removeFromTop (17);
        const int tab = line.indexOfChar ('\t');
        if (tab < 0)
        {
            g.setColour (pal::textDim);
            g.setFont (ui::font (10.5f));
            g.drawText (line, row, juce::Justification::centredLeft, true);
            continue;
        }

        g.setColour (pal::textFaint);
        g.setFont (ui::font (10.0f));
        g.drawText (line.substring (0, tab), row.removeFromLeft (84), juce::Justification::centredLeft, true);
        g.setColour (pal::text);
        g.setFont (ui::font (10.5f));
        g.drawText (line.substring (tab + 1), row, juce::Justification::centredLeft, true);
    }
}

void SearchOverlay::paintFooter (juce::Graphics& g)
{
    g.setColour (pal::sidebar);
    g.fillRect (footer);
    g.setColour (pal::divider);
    g.fillRect (footer.getX(), footer.getY(), footer.getWidth(), 1);

    auto r = footer.reduced (14, 0);
    r.removeFromRight (auditionBox.getWidth() + 10);

    const auto* e = selectedEntry();
    const auto action = (e != nullptr && host.describeAction) ? host.describeAction (*e) : juce::String();

    if (flash.isNotEmpty())
    {
        g.setColour (pal::accent);
        g.setFont (ui::font (11.0f, true));
        g.drawText (flash, r, juce::Justification::centredLeft, true);
    }
    else if (action.isNotEmpty())
    {
        auto key = r.removeFromLeft (44).withSizeKeepingCentre (40, 18);
        g.setColour (pal::field);
        g.fillRect (key);
        g.setColour (pal::textDim);
        g.setFont (ui::font (9.5f, true));
        g.drawText ("Enter", key, juce::Justification::centred);
        r.removeFromLeft (8);
        g.setColour (pal::text);
        g.setFont (ui::font (11.0f));
        g.drawText (action, r, juce::Justification::centredLeft, true);
    }
    else
    {
        g.setColour (pal::textFaint);
        g.setFont (ui::font (10.5f));
        g.drawText ("Up / Down to move   Enter to choose   Esc to close", r, juce::Justification::centredLeft, true);
    }

    // Audition toggle
    auto b = auditionBox;
    auto box = b.removeFromLeft (16).withSizeKeepingCentre (14, 14);
    g.setColour (pal::field);
    g.fillRect (box);
    g.setColour (audition ? pal::accent : pal::fieldEdge);
    g.drawRect (box, 1);
    if (audition)
    {
        g.setColour (pal::accent);
        g.fillRect (box.reduced (4));
    }
    b.removeFromLeft (6);
    g.setColour (audition ? pal::text : pal::textDim);
    g.setFont (ui::font (10.5f));
    g.drawText ("Audition", b, juce::Justification::centredLeft);
}
