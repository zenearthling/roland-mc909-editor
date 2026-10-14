#include "PartMixerPage.h"
#include "SearchIndex.h"
#include <map>
#include <tuple>

using namespace ui;

namespace
{
    const juce::Colour kDryColour (0xffe0a040), kRevColour (0xff7aa8ff);
    constexpr int kNumParts = 16;
    constexpr float kGap = 2.0f;

    const char* const kOutputNames[7] { "DRY", "MFX1", "MFX2", "COMP", "DIR1", "DIR2", "RHYTHM" };

    juce::String panText (int raw)
    {
        const int v = raw - 64;
        return v == 0 ? "C" : (v < 0 ? "L" + juce::String (-v) : "R" + juce::String (v));
    }
}

//==============================================================================
juce::String PartMixerBoard::lookUpName (int msb, int lsb, int program)
{
    using namespace mc909search;

    // (kind, bank, number) -> entry; built once.
    static const std::map<std::tuple<int, int, int>, const Entry*> table = []
    {
        std::map<std::tuple<int, int, int>, const Entry*> t;
        for (int i = 0; i < kNumEntries; ++i)
        {
            const auto& e = kEntries[i];
            if (e.kind == Kind::patch || e.kind == Kind::rhythmKit)
                t[{ (int) e.kind, e.bank, e.number }] = &e;
        }
        return t;
    }();

    const Entry* found = nullptr;
    if (msb == 81 && lsb >= 64 && lsb <= 70)
    {
        if (auto it = table.find ({ (int) Kind::patch, lsb - 64, program + 1 }); it != table.end())
            found = it->second;
    }
    else if (msb == 82 && lsb == 64)
    {
        if (auto it = table.find ({ (int) Kind::rhythmKit, 0, program + 1 }); it != table.end())
            found = it->second;
    }

    if (found != nullptr)
        return juce::String (juce::CharPointer_UTF8 (found->name));

    return "Bank " + juce::String (msb) + "/" + juce::String (lsb) + "  PC " + juce::String (program + 1);
}

//==============================================================================
PartMixerBoard::PartMixerBoard (MC909EditorProcessor& p)
    : ParamControl (p),
      rxDef (findParam ("part.rx_switch")), levelDef (findParam ("part.level")), panDef (findParam ("part.pan")),
      dryDef (findParam ("part.dry_send")), revDef (findParam ("part.rev_send")), outDef (findParam ("part.output"))
{
    setRepaintsOnMouseActivity (false);
}

PartMixerBoard::StripRects PartMixerBoard::rects (int part) const
{
    const auto all = getLocalBounds().toFloat();
    const float w = (all.getWidth() - kGap * (kNumParts - 1)) / (float) kNumParts;

    StripRects r;
    r.strip = { all.getX() + (float) part * (w + kGap), all.getY(), w, all.getHeight() };

    auto in = r.strip.reduced (3.0f, 4.0f);
    r.header = in.removeFromTop (46.0f);
    in.removeFromTop (3.0f);
    r.rx = in.removeFromTop (20.0f);
    in.removeFromTop (3.0f);
    r.output = in.removeFromTop (20.0f);
    in.removeFromTop (3.0f);
    r.pan = in.removeFromTop (22.0f);
    in.removeFromTop (6.0f);

    in.removeFromBottom (14.0f);                  // labels under the faders
    auto top = in.removeFromTop (14.0f);          // value readouts
    juce::ignoreUnused (top);

    const float barW = 17.0f;
    r.rev   = in.removeFromRight (barW);
    in.removeFromRight (2.0f);
    r.dry   = in.removeFromRight (barW);
    in.removeFromRight (2.0f);
    r.level = in;
    return r;
}

PartMixerBoard::Hit PartMixerBoard::hitTest (juce::Point<float> p) const
{
    for (int i = 0; i < kNumParts; ++i)
    {
        const auto r = rects (i);
        if (! r.strip.contains (p))
            continue;

        Hit h;
        h.part = i;
        h.area = r.rx.contains (p) ? rx
               : r.output.contains (p) ? output
               : r.pan.contains (p) ? pan
               : r.level.expanded (0.0f, 14.0f).contains (p) ? level
               : r.dry.expanded (0.0f, 14.0f).contains (p) ? dry
               : r.rev.expanded (0.0f, 14.0f).contains (p) ? rev
               : r.header.contains (p) ? header : none;
        return h;
    }
    return {};
}

juce::String PartMixerBoard::partTitle (int part) const
{
    if (! proc.havePartInfo (part) && part != proc.getSelectedPart())
        return "-";

    if (part == proc.getSelectedPart())
        if (const auto name = proc.getPatchName(); name.isNotEmpty())
            return name;

    return lookUpName (proc.getPartValue (part, findParam ("part.bank_msb")),
                       proc.getPartValue (part, findParam ("part.bank_lsb")),
                       proc.getPartValue (part, findParam ("part.program")));
}

//==============================================================================
void PartMixerBoard::paint (juce::Graphics& g)
{
    const auto accent = findColour (accentColourId, true);
    const int selected = proc.getSelectedPart();

    for (int i = 0; i < kNumParts; ++i)
    {
        const auto r = rects (i);
        const bool known = proc.havePartInfo (i);
        const bool on = proc.getPartValue (i, rxDef) != 0;
        const float live = known ? (on ? 1.0f : 0.55f) : 0.35f;

        g.setColour (col::card);
        g.fillRoundedRectangle (r.strip, 7.0f);
        g.setColour (i == selected ? accent : (i == hoverPart ? col::track : col::cardEdge));
        g.drawRoundedRectangle (r.strip.reduced (0.5f), 7.0f, i == selected ? 1.6f : 1.0f);

        // header: number and patch
        {
            g.setColour (i == selected ? accent : col::text);
            g.setFont (font (17.0f, true));
            g.drawText (juce::String (i + 1), r.header.withHeight (20.0f).toNearestInt(), juce::Justification::centred);

            g.setColour (col::textDim.withMultipliedAlpha (known || i == selected ? 1.0f : 0.5f));
            g.setFont (font (9.0f));
            g.drawFittedText (partTitle (i), r.header.withTrimmedTop (20.0f).toNearestInt(),
                              juce::Justification::centredTop, 2, 0.9f);
        }

        // receive switch
        {
            g.setColour (on && known ? accent.withAlpha (0.9f) : col::well);
            g.fillRoundedRectangle (r.rx, 5.0f);
            g.setColour (on && known ? col::bg : col::textDim);
            g.setFont (font (10.0f, true));
            g.drawText (known ? (on ? "RX ON" : "RX OFF") : "RX", r.rx.toNearestInt(), juce::Justification::centred);
        }

        // output select
        {
            const int o = juce::jlimit (0, 6, proc.getPartValue (i, outDef));
            g.setColour (col::well);
            g.fillRoundedRectangle (r.output, 5.0f);
            g.setColour (col::cardEdge);
            g.drawRoundedRectangle (r.output.reduced (0.5f), 5.0f, 1.0f);
            g.setColour (col::text.withMultipliedAlpha (known ? 1.0f : 0.4f));
            g.setFont (font (10.0f, true));
            g.setFont (font (9.0f, true));
            g.drawText (known ? kOutputNames[o] : "-", r.output.withTrimmedRight (7.0f).toNearestInt(), juce::Justification::centred, false);

            juce::Path tri;
            const auto c = juce::Point<float> (r.output.getRight() - 8.0f, r.output.getCentreY());
            tri.addTriangle (c.x - 3.0f, c.y - 2.0f, c.x + 3.0f, c.y - 2.0f, c.x, c.y + 2.5f);
            g.setColour (col::textDim);
            g.fillPath (tri);
        }

        // pan: centre-zero bar
        {
            const int v = proc.getPartValue (i, panDef);
            g.setColour (col::well);
            g.fillRoundedRectangle (r.pan, 5.0f);

            const float cx = r.pan.getCentreX();
            const float half = r.pan.getWidth() * 0.5f - 2.0f;
            const float x = cx + half * (float) (v - 64) / 64.0f;
            if (known)
            {
                g.setColour (accent.withAlpha (0.55f * live));
                g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (cx, x), r.pan.getY() + 2.0f,
                                                                 juce::jmax (2.0f, std::abs (x - cx)), r.pan.getHeight() - 4.0f), 3.0f);
            }
            g.setColour (col::textDim.withAlpha (0.6f));
            g.fillRect (cx - 0.5f, r.pan.getY() + 2.0f, 1.0f, r.pan.getHeight() - 4.0f);
            g.setColour (col::text.withMultipliedAlpha (known ? 1.0f : 0.4f));
            g.setFont (font (9.5f, true));
            g.drawText (known ? "PAN " + panText (v) : "PAN", r.pan.toNearestInt(), juce::Justification::centred);
        }

        // level fader
        {
            const int v = proc.getPartValue (i, levelDef);
            const auto readout = juce::Rectangle<float> (r.level.getX(), r.level.getY() - 14.0f, r.level.getWidth(), 14.0f);
            g.setColour (col::text.withMultipliedAlpha (known ? 1.0f : 0.4f));
            g.setFont (font (11.0f, true));
            g.drawText (known ? juce::String (v) : "-", readout.toNearestInt(), juce::Justification::centred);

            const float cx = r.level.getCentreX();
            auto track = juce::Rectangle<float> (4.0f, r.level.getHeight()).withCentre ({ cx, r.level.getCentreY() });
            g.setColour (col::well);
            g.fillRoundedRectangle (track, 2.0f);

            // scale ticks
            g.setColour (col::cardEdge);
            for (int k = 0; k <= 4; ++k)
            {
                const float y = r.level.getBottom() - r.level.getHeight() * (float) k / 4.0f;
                g.fillRect (cx - 9.0f, y - 0.5f, 5.0f, 1.0f);
                g.fillRect (cx + 4.0f, y - 0.5f, 5.0f, 1.0f);
            }

            const float y = r.level.getBottom() - r.level.getHeight() * (float) v / 127.0f;
            if (known)
            {
                g.setColour (accent.withAlpha (live));
                g.fillRoundedRectangle (juce::Rectangle<float> (cx - 2.0f, y, 4.0f, r.level.getBottom() - y), 2.0f);

                auto cap = juce::Rectangle<float> (20.0f, 10.0f).withCentre ({ cx, y });
                g.setColour (col::knobBody.brighter (0.15f));
                g.fillRoundedRectangle (cap, 3.0f);
                g.setColour (accent.withAlpha (live));
                g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 0.75f, cap.getWidth() - 6.0f, 1.5f);
            }
        }

        // dry and reverb send bars
        const auto sendBar = [&] (juce::Rectangle<float> bar, int v, juce::Colour c, const char* label)
        {
            g.setColour (col::well);
            g.fillRoundedRectangle (bar, 3.0f);
            if (known)
            {
                const float h = bar.getHeight() * (float) v / 127.0f;
                g.setColour (c.withAlpha (0.85f * live));
                g.fillRoundedRectangle (bar.withTrimmedTop (bar.getHeight() - h).reduced (1.0f, 0.0f).withTrimmedBottom (1.0f), 2.0f);
            }
            g.setColour (col::text.withMultipliedAlpha (known ? 1.0f : 0.4f));
            g.setFont (font (8.5f, true));
            g.drawText (known ? juce::String (v) : "-", bar.withY (bar.getY() - 14.0f).withHeight (14.0f).expanded (2.0f, 0.0f).toNearestInt(),
                        juce::Justification::centred);
            g.setColour (c.withAlpha (0.9f));
            g.setFont (font (8.0f, true));
            g.drawText (label, bar.withY (bar.getBottom() + 1.0f).withHeight (13.0f).expanded (3.0f, 0.0f).toNearestInt(),
                        juce::Justification::centred, false);
        };
        sendBar (r.dry, proc.getPartValue (i, dryDef), kDryColour, "DRY");
        sendBar (r.rev, proc.getPartValue (i, revDef), kRevColour, "REV");

        g.setColour (col::textDim);
        g.setFont (font (8.0f, true));
        g.drawText ("LVL", r.level.withY (r.level.getBottom() + 1.0f).withHeight (13.0f).toNearestInt(), juce::Justification::centred, false);
    }
}

//==============================================================================
void PartMixerBoard::choosePart (int part)
{
    if (part == proc.getSelectedPart())
        return;

    if (onPartPicked)
        onPartPicked (part);
    else
        proc.setSelectedPart (part);
}

void PartMixerBoard::setOutput (int part, int value)
{
    proc.setPartValue (part, outDef, value);
}

void PartMixerBoard::applyDrag (juce::Point<float> p)
{
    if (dragPart < 0 || dragWhat == none)
        return;

    const auto r = rects (dragPart);
    const auto fromBottom = [] (juce::Rectangle<float> a, float y) { return juce::jlimit (0.0f, 1.0f, (a.getBottom() - y) / a.getHeight()); };

    switch (dragWhat)
    {
        case level: proc.setPartValue (dragPart, levelDef, juce::roundToInt (127.0f * fromBottom (r.level, p.y))); break;
        case dry:   proc.setPartValue (dragPart, dryDef,   juce::roundToInt (127.0f * fromBottom (r.dry,   p.y))); break;
        case rev:   proc.setPartValue (dragPart, revDef,   juce::roundToInt (127.0f * fromBottom (r.rev,   p.y))); break;
        case pan:
        {
            const float half = r.pan.getWidth() * 0.5f - 2.0f;
            const float t = juce::jlimit (-1.0f, 1.0f, (p.x - r.pan.getCentreX()) / half);
            const int v = juce::roundToInt (64.0f + 64.0f * t);
            proc.setPartValue (dragPart, panDef, juce::jlimit (0, 127, std::abs (v - 64) <= 3 ? 64 : v));   // a little magnet at the centre
            break;
        }
        default: break;
    }
}

void PartMixerBoard::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTest (e.position).part;
    if (h != hoverPart)
    {
        hoverPart = h;
        repaint();
    }
}

void PartMixerBoard::mouseDown (const juce::MouseEvent& e)
{
    const auto hit = hitTest (e.position);
    dragWhat = none;
    dragPart = hit.part;

    if (hit.part < 0)
        return;

    choosePart (hit.part);

    switch (hit.area)
    {
        case rx:
            proc.setPartValue (hit.part, rxDef, proc.getPartValue (hit.part, rxDef) != 0 ? 0 : 1);
            break;

        case output:
        {
            juce::PopupMenu m;
            const int current = proc.getPartValue (hit.part, outDef);
            for (int o = 0; o < 7; ++o)
                m.addItem (o + 1, kOutputNames[o], true, o == current);

            const int part = hit.part;
            juce::Component::SafePointer<PartMixerBoard> safe (this);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (rects (part).output.toNearestInt())),
                             [safe, part] (int result)
                             {
                                 if (safe != nullptr && result > 0)
                                     safe->setOutput (part, result - 1);
                             });
            break;
        }

        case level: case dry: case rev: case pan:
            dragWhat = hit.area;
            applyDrag (e.position);
            break;

        default: break;
    }
}

void PartMixerBoard::mouseDrag (const juce::MouseEvent& e)
{
    applyDrag (e.position);
}

void PartMixerBoard::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto hit = hitTest (e.position);
    if (hit.part < 0)
        return;

    if (hit.area == pan)   proc.setPartValue (hit.part, panDef, 64);
    if (hit.area == level) proc.setPartValue (hit.part, levelDef, 100);
}

void PartMixerBoard::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const auto hit = hitTest (e.position);
    const int step = (w.deltaY > 0 ? 1 : -1) * (e.mods.isShiftDown() ? 1 : 4);

    const mc909::ParamDef* d = hit.area == level ? &levelDef : hit.area == dry ? &dryDef
                             : hit.area == rev ? &revDef : hit.area == pan ? &panDef : nullptr;
    if (hit.part >= 0 && d != nullptr && w.deltaY != 0.0f)
        proc.setPartValue (hit.part, *d, proc.getPartValue (hit.part, *d) + step);
}

const mc909::ParamDef* PartMixerBoard::teachDef (juce::Point<int> p) const
{
    // Macros act on the selected part, so only that strip can be taught.
    const auto hit = hitTest (p.toFloat());
    if (hit.part != proc.getSelectedPart())
        return nullptr;

    switch (hit.area)
    {
        case rx: return &rxDef;
        case output: return &outDef;
        case pan: return &panDef;
        case level: return &levelDef;
        case dry: return &dryDef;
        case rev: return &revDef;
        default: return nullptr;
    }
}

//==============================================================================
PartMixerPage::PartMixerPage (MC909EditorProcessor& p) : PageBase (p)
{
    board = add<PartMixerBoard>();

    coarse   = add<ParamKnob>   ("part.coarse", "Coarse Tune");
    fine     = add<ParamKnob>   ("part.fine", "Fine Tune");
    autoSync = add<ParamToggle> ("part.auto_sync", "Auto Sync");
    bankMsb  = add<ParamNumber> ("part.bank_msb", "Bank MSB");
    bankLsb  = add<ParamNumber> ("part.bank_lsb", "Bank LSB");
    program  = add<ParamNumber> ("part.program", "Program");

    addAndMakeVisible (readButton);
    readButton.setTooltip ("Ask the MC-909 for the level, pan, sends and patch of all 16 parts");
    readButton.onClick = [this] { proc.requestPartInfo(); };
}

void PartMixerPage::visibilityChanged()
{
    // First time the mixer is shown, fetch every part so the strips are filled in.
    if (isVisible() && ! proc.haveAllPartInfo())
        proc.requestPartInfo();
}

void PartMixerPage::resized()
{
    cards.clear();

    auto r = getLocalBounds().reduced (6);
    auto top = r.removeFromTop (24);
    readButton.setBounds (top.removeFromRight (120).reduced (0, 1));
    noteArea = top;
    r.removeFromTop (4);

    constexpr int detailH = 112;
    detailCard = r.removeFromBottom (detailH);
    r.removeFromBottom (8);
    board->setBounds (r);

    cards.push_back ({ detailCard, "Selected part" });
    auto inner = contentOf (cards.back());
    coarse->setBounds (inner.removeFromLeft (ParamKnob::cellWidth + 20).withHeight (ParamKnob::cellHeight));
    fine->setBounds   (inner.removeFromLeft (ParamKnob::cellWidth + 20).withHeight (ParamKnob::cellHeight));
    inner.removeFromLeft (16);
    autoSync->setBounds (inner.removeFromLeft (90).withTrimmedTop (14).withHeight (46));
    inner.removeFromLeft (30);
    for (auto* n : { bankMsb, bankLsb, program })
    {
        n->setBounds (inner.removeFromLeft (96).withTrimmedTop (14).withHeight (50));
        inner.removeFromLeft (10);
    }
}

void PartMixerPage::paint (juce::Graphics& g)
{
    if (! cards.empty())
        cards.back().title = "Part " + juce::String (proc.getSelectedPart() + 1) + " - tuning, bank and program";

    PageBase::paint (g);

    g.setColour (col::textDim.withAlpha (0.9f));
    g.setFont (font (10.5f));
    g.drawFittedText ("Drag a fader, pan bar or send. Double-click pan or level to reset. Click a strip to select that part. "
                      "Part names come from the bank and program the unit reports.",
                      noteArea, juce::Justification::centredLeft, 1);
}
