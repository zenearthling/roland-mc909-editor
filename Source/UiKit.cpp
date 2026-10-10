#include "UiKit.h"
#include <cmath>
#include <cstdlib>
#include <map>

namespace ui
{

using namespace mc909;

//==============================================================================
juce::Colour toneColour (int tone)
{
    static const juce::Colour c[4] {
        juce::Colour (0xff2fd9c4),   // tone 1  teal
        juce::Colour (0xffa881ff),   // tone 2  violet
        juce::Colour (0xffffb454),   // tone 3  amber
        juce::Colour (0xffff6fa8)    // tone 4  pink
    };
    return c[juce::jlimit (0, 3, tone)];
}

juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

static float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

const ParamDef& findParam (juce::StringRef id)
{
    static const auto lookup = []
    {
        std::map<juce::String, const ParamDef*> m;
        for (const auto& p : allParams())
            m[p.id] = &p;
        return m;
    }();

    const auto it = lookup.find (juce::String (id));
    jassert (it != lookup.end());   // a typo'd id should be loud in debug builds
    return it != lookup.end() ? *it->second : allParams().front();
}

//==============================================================================
SynthLookAndFeel::SynthLookAndFeel()
{
    setColour (accentColourId, toneColour (0));
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);

    setColour (juce::Label::textColourId, col::text);

    setColour (juce::ComboBox::backgroundColourId, col::well);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::cardEdge);
    setColour (juce::ComboBox::arrowColourId, col::textDim);

    setColour (juce::PopupMenu::backgroundColourId, col::card);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff2c3340));
    setColour (juce::PopupMenu::highlightedTextColourId, col::text);

    setColour (juce::TextEditor::backgroundColourId, col::well);
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::outlineColourId, col::cardEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, toneColour (0));
    setColour (juce::TextEditor::highlightColourId, juce::Colour (0xff2c3340));
    setColour (juce::CaretComponent::caretColourId, col::text);

    setColour (juce::TextButton::buttonColourId, col::knobBody);
    setColour (juce::TextButton::buttonOnColourId, toneColour (0));
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, col::bg);

    setColour (juce::ToggleButton::textColourId, col::textDim);

    setColour (juce::ScrollBar::thumbColourId, col::track);
    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
}

void SynthLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto accent = slider.findColour (accentColourId, true);
    const bool enabled = slider.isEnabled();

    const float arcRadius = radius - 1.5f;
    const float stroke = 3.0f;

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (col::track);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc: from the zero point for bipolar controls, otherwise from the start.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroPos = bipolar ? (float) slider.valueToProportionOfLength (0.0) : 0.0f;
    const float fromAngle = startAngle + zeroPos * (endAngle - startAngle);
    const float toAngle   = startAngle + pos * (endAngle - startAngle);

    if (std::abs (toAngle - fromAngle) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                           juce::jmin (fromAngle, toAngle), juce::jmax (fromAngle, toAngle), true);
        g.setColour (enabled ? accent : accent.withAlpha (0.3f));
        g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Knob body
    const float bodyRadius = arcRadius - 6.0f;
    juce::ColourGradient grad (col::knobBody.brighter (0.18f), centre.x, centre.y - bodyRadius,
                               col::knobBody.darker (0.25f),   centre.x, centre.y + bodyRadius, false);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (centre.x - bodyRadius, centre.y - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

    // Pointer
    const float ps = std::sin (toAngle), pc = std::cos (toAngle);
    g.setColour (col::text);
    g.drawLine (centre.x + ps * bodyRadius * 0.32f, centre.y - pc * bodyRadius * 0.32f,
                centre.x + ps * bodyRadius * 0.86f, centre.y - pc * bodyRadius * 0.86f, 2.0f);
}

void SynthLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float minPos, float maxPos, juce::Slider::SliderStyle style,
                                         juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, slider);
        return;
    }

    const auto accent = slider.findColour (accentColourId, true);
    const float cy = (float) y + (float) h * 0.5f;
    const juce::Rectangle<float> track ((float) x, cy - 2.0f, (float) w, 4.0f);

    g.setColour (col::track);
    g.fillRoundedRectangle (track, 2.0f);

    // Fill from the zero point on bipolar sliders, otherwise from the left edge.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float zeroX = bipolar ? (float) x + (float) slider.valueToProportionOfLength (0.0) * (float) w
                                : (float) x;
    const float from = juce::jmin (zeroX, pos), to = juce::jmax (zeroX, pos);

    g.setColour (accent.withAlpha (0.85f));
    g.fillRoundedRectangle (juce::Rectangle<float> (from, cy - 2.0f, to - from, 4.0f), 2.0f);

    g.setColour (col::text);
    g.fillEllipse (pos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
    g.setColour (accent);
    g.fillEllipse (pos - 4.0f, cy - 4.0f, 8.0f, 8.0f);
}

juce::Label* SynthLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (11.5f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, col::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textWhenEditingColourId, col::text);
    l->setColour (juce::Label::backgroundWhenEditingColourId, col::well);
    l->setColour (juce::Label::outlineWhenEditingColourId, col::cardEdge);
    return l;
}

void SynthLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int,
                                     juce::ComboBox& box)
{
    const auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (down ? box.findColour (accentColourId, true) : box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (b, 5.0f, 1.0f);

    const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font SynthLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font (12.5f);
}

void SynthLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void SynthLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& background, bool highlighted, bool down)
{
    const auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    auto c = background;
    if (down)             c = c.darker (0.15f);
    else if (highlighted) c = c.brighter (0.12f);

    g.setColour (c);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (button.getToggleState() ? c.brighter (0.25f) : col::cardEdge);
    g.drawRoundedRectangle (b, 5.0f, 1.0f);
}

juce::Font SynthLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return font (12.5f, true);
}

void SynthLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto b = button.getLocalBounds().toFloat();
    const float s = juce::jmin (16.0f, b.getHeight() - 4.0f);
    const juce::Rectangle<float> led (b.getX() + 2.0f, b.getCentreY() - s * 0.5f, s, s);
    const auto accent = button.findColour (accentColourId, true);
    const bool on = button.getToggleState();

    g.setColour (on ? accent : col::well);
    g.fillRoundedRectangle (led, 3.0f);
    g.setColour (on ? accent.brighter (0.3f) : (highlighted ? col::textDim : col::cardEdge));
    g.drawRoundedRectangle (led, 3.0f, 1.0f);

    g.setColour (on ? col::text : col::textDim);
    g.setFont (font (12.0f));
    g.drawText (button.getButtonText(), b.withTrimmedLeft (s + 7.0f).toNearestInt(), juce::Justification::centredLeft);
}

void SynthLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g, bool over, bool)
{
    auto area = button.getActiveArea();
    const bool front = button.isFrontTab();
    const auto accent = button.findColour (accentColourId, true);

    if (over && ! front)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (area.toFloat().reduced (2.0f, 3.0f), 5.0f);
    }

    g.setColour (front ? col::text : col::textDim);
    g.setFont (font (12.5f, front));
    g.drawText (button.getButtonText().toUpperCase(), area, juce::Justification::centred);

    if (front)
    {
        g.setColour (accent);
        g.fillRect (area.removeFromBottom (2).reduced (10, 0));
    }
}

int SynthLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int)
{
    return (int) textWidth (font (12.5f, true), button.getButtonText().toUpperCase()) + 30;
}

void SynthLookAndFeel::drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics& g, int w, int h)
{
    g.setColour (col::cardEdge);
    g.fillRect (0, h - 1, w, 1);
}

//==============================================================================
ParamKnob::ParamKnob (MC909EditorProcessor& p, const juce::String& id, const juce::String& caption)
    : ParamControl (p), def (findParam (id))
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 15);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setRange (def.toDisplay (def.rawMin), def.toDisplay (def.rawMax), juce::jmax (1, def.dispMul));
    slider.setMouseDragSensitivity (220);
    slider.setNumDecimalPlacesToDisplay (0);

    const double lo = def.toDisplay (def.rawMin), hi = def.toDisplay (def.rawMax);
    const bool bipolar = lo < 0.0 && hi > 0.0;
    slider.setDoubleClickReturnValue (true, bipolar ? 0.0 : lo);

    if (bipolar)
        slider.textFromValueFunction = [] (double v) { return (v > 0.0 ? "+" : "") + juce::String ((int) v); };

    slider.addListener (this);
    addAndMakeVisible (slider);

    name.setText (caption.isEmpty() ? def.name : caption, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setFont (font (11.0f));
    name.setColour (juce::Label::textColourId, col::textDim);
    name.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (name);

    refresh();
}

void ParamKnob::resized()
{
    auto r = getLocalBounds();
    name.setBounds (r.removeFromBottom (14));
    slider.setBounds (r);
}

void ParamKnob::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    slider.setValue (def.toDisplay (proc.getParameterValue (def)), juce::dontSendNotification);
}

void ParamKnob::sliderValueChanged (juce::Slider*)
{
    if (updating)
        return;

    proc.setParameterValue (def, def.fromDisplay ((int) std::lround (slider.getValue())));
}

//==============================================================================
ParamChoice::ParamChoice (MC909EditorProcessor& p, const juce::String& id,
                          const juce::String& caption, bool compact)
    : ParamControl (p), def (findParam (id)), compactMode (compact)
{
    name.setText (caption.isEmpty() ? def.name : caption, juce::dontSendNotification);
    name.setFont (font (11.0f));
    name.setColour (juce::Label::textColourId, col::textDim);
    name.setInterceptsMouseClicks (false, false);
    name.setVisible (! compactMode);
    addChildComponent (name);

    for (int i = 0; i < def.choices.size(); ++i)
        combo.addItem (def.choices[i], i + 1);

    combo.addListener (this);
    addAndMakeVisible (combo);
    refresh();
}

void ParamChoice::resized()
{
    auto r = getLocalBounds();

    if (compactMode)
    {
        combo.setBounds (r);
        return;
    }

    name.setBounds (r.removeFromTop (14));
    combo.setBounds (r.removeFromTop (24));
}

void ParamChoice::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    combo.setSelectedId (juce::jlimit (1, juce::jmax (1, def.choices.size()),
                                       proc.getParameterValue (def) - def.rawMin + 1),
                         juce::dontSendNotification);
}

void ParamChoice::comboBoxChanged (juce::ComboBox*)
{
    if (updating)
        return;

    proc.setParameterValue (def, def.rawMin + combo.getSelectedId() - 1);
}

//==============================================================================
ParamToggle::ParamToggle (MC909EditorProcessor& p, const juce::String& id, const juce::String& cap)
    : ParamControl (p), def (findParam (id)), caption (cap.isEmpty() ? def.name : cap)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    refresh();
}

void ParamToggle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto accent = findColour (accentColourId, true);

    g.setColour (col::textDim);
    g.setFont (font (11.0f));
    g.drawText (caption, r.removeFromTop (14.0f).toNearestInt(), juce::Justification::centredLeft);

    auto pill = r.removeFromTop (24.0f).reduced (0.5f);
    g.setColour (on ? accent : col::well);
    g.fillRoundedRectangle (pill, 5.0f);
    g.setColour (on ? accent.brighter (0.3f) : col::cardEdge);
    g.drawRoundedRectangle (pill, 5.0f, 1.0f);

    g.setColour (on ? col::bg : col::textDim);
    g.setFont (font (12.0f, true));
    g.drawText (on ? "ON" : "OFF", pill.toNearestInt(), juce::Justification::centred);
}

void ParamToggle::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;

    on = ! on;
    proc.setParameterValue (def, on ? 1 : 0);
    repaint();
}

void ParamToggle::refresh()
{
    on = proc.getParameterValue (def) != 0;
    repaint();
}

//==============================================================================
ParamNumber::ParamNumber (MC909EditorProcessor& p, const juce::String& id, const juce::String& cap)
    : ParamControl (p), def (findParam (id)), caption (cap.isEmpty() ? def.name : cap)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);

    editor.setMultiLine (false);
    editor.setInputRestrictions (6, "0123456789");
    editor.setJustification (juce::Justification::centred);
    editor.onReturnKey = [this] { commit (editor.getText().getIntValue()); editor.setVisible (false); };
    editor.onEscapeKey = [this] { editor.setVisible (false); };
    editor.onFocusLost = [this] { editor.setVisible (false); };
    addChildComponent (editor);

    refresh();
}

void ParamNumber::resized()
{
    editor.setBounds (getLocalBounds().withTrimmedTop (14).withHeight (24));
}

void ParamNumber::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::textDim);
    g.setFont (font (11.0f));
    g.drawText (caption, r.removeFromTop (14.0f).toNearestInt(), juce::Justification::centredLeft);

    auto box = r.removeFromTop (24.0f).reduced (0.5f);
    g.setColour (col::well);
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (col::cardEdge);
    g.drawRoundedRectangle (box, 5.0f, 1.0f);

    g.setColour (col::text);
    g.setFont (font (13.5f, true));
    g.drawText (juce::String (value), box.toNearestInt(), juce::Justification::centred);
}

void ParamNumber::commit (int raw)
{
    raw = juce::jlimit (def.rawMin, def.rawMax, raw);
    if (raw != proc.getParameterValue (def))
        proc.setParameterValue (def, raw);

    value = raw;
    repaint();
}

void ParamNumber::mouseDown (const juce::MouseEvent&)
{
    dragStart = proc.getParameterValue (def);
}

void ParamNumber::mouseDrag (const juce::MouseEvent& e)
{
    const int span = def.rawMax - def.rawMin;
    const int step = e.mods.isShiftDown() ? 1 : juce::jmax (1, span / 400);
    commit (dragStart + (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) * step);
}

void ParamNumber::mouseDoubleClick (const juce::MouseEvent&)
{
    editor.setText (juce::String (value), false);
    editor.setVisible (true);
    editor.grabKeyboardFocus();
    editor.selectAll();
}

void ParamNumber::refresh()
{
    value = proc.getParameterValue (def);
    repaint();
}

//==============================================================================
EnvelopeEditor::EnvelopeEditor (MC909EditorProcessor& p, Kind k, const juce::String& root)
    : ParamControl (p), kind (k)
{
    const juce::String pre = root + (kind == Kind::pitch ? "penv_" : kind == Kind::filter ? "tvf_" : "tva_");

    for (int i = 0; i < 4; ++i)
        timeDef[i] = &findParam (pre + "t" + juce::String (i + 1));

    if (kind == Kind::amp)
    {
        // Start and end are fixed at zero; only levels 1-3 exist in the map.
        for (int i = 1; i <= 3; ++i)
            levelDef[i] = &findParam (pre + "l" + juce::String (i));
    }
    else
    {
        for (int i = 0; i < 5; ++i)
            levelDef[i] = &findParam (pre + "l" + juce::String (i));
    }

    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> EnvelopeEditor::plot() const
{
    return getLocalBounds().toFloat().reduced (12.0f, 14.0f);
}

float EnvelopeEditor::levelNorm (int node) const
{
    if (levelDef[node] == nullptr)
        return 0.0f;

    const auto& d = *levelDef[node];
    const float raw = (float) proc.getParameterValue (d);
    return juce::jlimit (0.0f, 1.0f, (raw - (float) d.rawMin) / (float) (d.rawMax - d.rawMin));
}

juce::Point<float> EnvelopeEditor::nodePos (int node) const
{
    const auto p = plot();
    const float slot = p.getWidth() / 4.0f;

    float x = p.getX();
    for (int k = 1; k <= node; ++k)
        x += slot * (float) proc.getParameterValue (*timeDef[k - 1]) / 127.0f;

    return { x, p.getBottom() - levelNorm (node) * p.getHeight() };
}

bool EnvelopeEditor::nodeMovable (int node) const
{
    return levelDef[node] != nullptr || node >= 1;
}

int EnvelopeEditor::hitTest (juce::Point<float> pt) const
{
    int best = -1;
    float bestDist = 11.0f;

    for (int i = 0; i < 5; ++i)
    {
        if (! nodeMovable (i))
            continue;

        const float d = nodePos (i).getDistanceFrom (pt);
        if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
}

const ParamDef* EnvelopeEditor::teachDef (juce::Point<int> p) const
{
    const int n = hitTest (p.toFloat());
    return n >= 0 ? levelDef[n] : nullptr;
}

void EnvelopeEditor::setIfChanged (const ParamDef& d, int raw)
{
    raw = juce::jlimit (d.rawMin, d.rawMax, raw);
    if (proc.getParameterValue (d) != raw)
        proc.setParameterValue (d, raw);
}

juce::String EnvelopeEditor::describe (int node) const
{
    juce::String s;

    if (node >= 1)
        s << "T" << node << " " << proc.getParameterValue (*timeDef[node - 1]) << "   ";

    if (levelDef[node] != nullptr)
    {
        const int raw = proc.getParameterValue (*levelDef[node]);
        s << "L" << node << " " << (kind == Kind::pitch ? juce::String (raw - 64) : juce::String (raw));
    }
    else
    {
        s << "L" << node << " 0";
    }

    return s.trim();
}

void EnvelopeEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto accent = findColour (accentColourId, true);
    const auto p = plot();

    g.setColour (col::well);
    g.fillRoundedRectangle (b, 6.0f);

    // Grid: quarter divisions, plus the zero line for the bipolar pitch envelope.
    g.setColour (col::cardEdge.withAlpha (0.55f));
    for (int k = 1; k < 4; ++k)
        g.fillRect (p.getX() + p.getWidth() * (float) k / 4.0f, b.getY() + 6.0f, 1.0f, b.getHeight() - 12.0f);

    const float baseY = kind == Kind::pitch ? p.getCentreY() : p.getBottom();
    g.setColour (col::cardEdge);
    g.fillRect (p.getX() - 4.0f, baseY, p.getWidth() + 8.0f, 1.0f);

    juce::Path line;
    for (int i = 0; i < 5; ++i)
    {
        const auto pt = nodePos (i);
        if (i == 0) line.startNewSubPath (pt);
        else        line.lineTo (pt);
    }

    juce::Path fill (line);
    fill.lineTo (nodePos (4).x, baseY);
    fill.lineTo (nodePos (0).x, baseY);
    fill.closeSubPath();

    g.setColour (accent.withAlpha (0.16f));
    g.fillPath (fill);

    g.setColour (accent);
    g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    for (int i = 0; i < 5; ++i)
    {
        if (! nodeMovable (i))
            continue;

        const auto pt = nodePos (i);
        const bool active = (i == hoverNode || i == dragNode);
        const float r = active ? 6.0f : 4.5f;

        g.setColour (col::well);
        g.fillEllipse (pt.x - r - 1.5f, pt.y - r - 1.5f, (r + 1.5f) * 2.0f, (r + 1.5f) * 2.0f);
        g.setColour (active ? juce::Colours::white : accent);
        g.fillEllipse (pt.x - r, pt.y - r, r * 2.0f, r * 2.0f);
    }

    const int shown = dragNode >= 0 ? dragNode : hoverNode;
    if (shown >= 0)
    {
        g.setColour (col::text);
        g.setFont (font (11.5f, true));
        g.drawText (describe (shown), getLocalBounds().reduced (8, 3), juce::Justification::topLeft);
    }
}

void EnvelopeEditor::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTest (e.position);
    if (h != hoverNode)
    {
        hoverNode = h;
        repaint();
    }
}

void EnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    dragNode = hitTest (e.position);
    repaint();
}

void EnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0)
        return;

    const auto p = plot();

    if (levelDef[dragNode] != nullptr)
    {
        const auto& d = *levelDef[dragNode];
        float norm = juce::jlimit (0.0f, 1.0f, (p.getBottom() - e.position.y) / p.getHeight());

        if (kind == Kind::pitch && std::abs (norm - 0.5f) < 0.025f)
            norm = 0.5f;   // snap to the zero line

        setIfChanged (d, d.rawMin + juce::roundToInt (norm * (float) (d.rawMax - d.rawMin)));
    }

    if (dragNode >= 1)
    {
        const float slot = p.getWidth() / 4.0f;
        const float f = juce::jlimit (0.0f, 1.0f, (e.position.x - nodePos (dragNode - 1).x) / slot);
        setIfChanged (*timeDef[dragNode - 1], juce::roundToInt (f * 127.0f));
    }

    repaint();
}

void EnvelopeEditor::mouseUp (const juce::MouseEvent&)
{
    dragNode = -1;
    repaint();
}

void EnvelopeEditor::mouseExit (const juce::MouseEvent&)
{
    hoverNode = -1;
    repaint();
}

//==============================================================================
FilterCurve::FilterCurve (MC909EditorProcessor& p, const juce::String& root)
    : ParamControl (p),
      typeDef (findParam (root + "tvf_type")),
      cutoffDef (findParam (root + "tvf_cutoff")),
      resoDef (findParam (root + "tvf_reso"))
{
}

static double filterResponse (int type, double f, double fc, double q)
{
    const double x = f / fc;
    const double a = 1.0 - x * x;
    const double bq = x / q;
    const double lp = 1.0 / std::sqrt (a * a + bq * bq);   // 2-pole low-pass; peaks at q when x = 1

    switch (type)
    {
        case 1: return lp;                                     // LPF
        case 2: return (x / q) * lp;                           // BPF, unity at the cutoff
        case 3: return x * x * lp;                             // HPF
        case 4:                                                // PKG: a peak that grows with resonance
        {
            const double bp = (x / q) * lp;
            const double gain = 1.0 + (q - 0.707) * 0.9;
            return std::sqrt (1.0 + (gain * gain - 1.0) * bp * bp);
        }
        case 5: return lp / std::sqrt (1.0 + x * x);           // LPF2: one extra pole
        case 6: return lp / std::sqrt (1.0 + x * x * x * x);   // LPF3: two extra poles
        default: return 1.0;                                   // OFF
    }
}

void FilterCurve::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto accent = findColour (accentColourId, true);
    const auto area = b.reduced (6.0f, 8.0f);

    g.setColour (col::well);
    g.fillRoundedRectangle (b, 6.0f);

    const int type = proc.getParameterValue (typeDef);
    const double cutoff = (double) proc.getParameterValue (cutoffDef);
    const double reso = (double) proc.getParameterValue (resoDef);

    const double fc = 20.0 * std::pow (1000.0, cutoff / 127.0);
    const double q = 0.707 * std::pow (10.0, (reso / 127.0) * 1.1);

    constexpr double dbTop = 24.0, dbBottom = -42.0;
    auto yForDb = [&] (double db)
    {
        return (float) (area.getBottom() - (db - dbBottom) / (dbTop - dbBottom) * (double) area.getHeight());
    };
    auto xForHz = [&] (double hz)
    {
        return (float) ((double) area.getX() + std::log (hz / 20.0) / std::log (1000.0) * (double) area.getWidth());
    };

    g.setFont (font (10.0f));
    for (const double hz : { 100.0, 1000.0, 10000.0 })
    {
        const float x = xForHz (hz);
        g.setColour (col::cardEdge.withAlpha (0.6f));
        g.fillRect (x, b.getY() + 4.0f, 1.0f, b.getHeight() - 8.0f);
        g.setColour (col::textDim.withAlpha (0.8f));
        g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (x + 3.0f, b.getBottom() - 15.0f, 28.0f, 12.0f).toNearestInt(),
                    juce::Justification::centredLeft);
    }

    g.setColour (col::cardEdge);
    g.fillRect (b.getX() + 4.0f, yForDb (0.0), b.getWidth() - 8.0f, 1.0f);

    juce::Path curve;
    const int steps = juce::jmax (8, (int) area.getWidth() / 2);
    for (int i = 0; i <= steps; ++i)
    {
        const double t = (double) i / (double) steps;
        const double hz = 20.0 * std::pow (1000.0, t);
        const double mag = filterResponse (type, hz, fc, q);
        const double db = juce::jlimit (dbBottom, dbTop, 20.0 * std::log10 (juce::jmax (mag, 1.0e-6)));
        const float x = area.getX() + (float) t * area.getWidth();

        if (i == 0) curve.startNewSubPath (x, yForDb (db));
        else        curve.lineTo (x, yForDb (db));
    }

    juce::Path fill (curve);
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();

    g.setColour (accent.withAlpha (0.16f));
    g.fillPath (fill);

    if (type != 0)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillRect (xForHz (fc), b.getY() + 4.0f, 1.0f, b.getHeight() - 8.0f);
    }

    g.setColour (accent);
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
LfoScope::LfoScope (MC909EditorProcessor& p, int lfoIndex)
    : ParamControl (p), prefix ("tone.lfo" + juce::String (lfoIndex) + ".")
{
}

int LfoScope::raw (const char* suffix) const
{
    return proc.getParameterValue (findParam (prefix + suffix));
}

void LfoScope::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto accent = findColour (accentColourId, true);
    const auto area = b.reduced (8.0f, 8.0f);

    g.setColour (col::well);
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (col::cardEdge);
    g.fillRect (area.getX() - 4.0f, area.getCentreY(), area.getWidth() + 8.0f, 1.0f);

    const float delayFrac = (float) raw ("delay") / 127.0f * 0.35f;
    const int   fadeMode  = raw ("fade_mode");           // 0 ON-IN, 1 ON-OUT, 2 OFF-IN, 3 OFF-OUT
    const float fadeFrac  = juce::jmax (0.02f, (float) raw ("fade_time") / 127.0f * 0.45f);
    const float cycles    = 1.5f + (float) juce::jmin (127, raw ("rate")) / 127.0f * 9.0f;

    int deepest = 0;
    for (const char* s : { "pitch_dep", "tvf_dep", "tva_dep", "pan_dep" })
        deepest = juce::jmax (deepest, std::abs (raw (s) - 64));
    const float amp = juce::jmax (0.14f, (float) deepest / 63.0f);

    juce::Path wave;
    const int steps = juce::jmax (16, (int) area.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;

        float env = 1.0f;
        if (t < delayFrac)
            env = 0.0f;
        else if (fadeMode == 0 || fadeMode == 2)
            env = juce::jlimit (0.0f, 1.0f, (t - delayFrac) / fadeFrac);
        else
            env = 1.0f - juce::jlimit (0.0f, 1.0f, (t - delayFrac) / fadeFrac);

        const float y = area.getCentreY()
                      - amp * 0.9f * (area.getHeight() * 0.5f) * env
                          * std::sin (juce::MathConstants<float>::twoPi * cycles * t);
        const float x = area.getX() + t * area.getWidth();

        if (i == 0) wave.startNewSubPath (x, y);
        else        wave.lineTo (x, y);
    }

    g.setColour (accent);
    g.strokePath (wave, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void PageBase::refresh()
{
    for (auto* c : controls)
        c->refresh();

    repaint();
}

void PageBase::paint (juce::Graphics& g)
{
    const auto accent = findColour (accentColourId, true);

    for (const auto& c : cards)
    {
        const auto r = c.area.toFloat();

        g.setColour (col::card);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::cardEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (accent);
        g.fillEllipse (r.getX() + 12.0f, r.getY() + 11.0f, 6.0f, 6.0f);

        g.setColour (col::textDim);
        g.setFont (font (11.0f, true).withExtraKerningFactor (0.14f));
        g.drawText (c.title.toUpperCase(), (int) r.getX() + 24, (int) r.getY() + 6, c.area.getWidth() - 30, 16,
                    juce::Justification::centredLeft);
    }
}

juce::Rectangle<int> PageBase::contentOf (const Card& c)
{
    return c.area.reduced (10, 8).withTrimmedTop (22);
}

void PageBase::placeKnobs (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int columns)
{
    int i = 0;
    for (auto* c : items)
    {
        c->setBounds (area.getX() + (i % columns) * ParamKnob::cellWidth,
                      area.getY() + (i / columns) * ParamKnob::cellHeight,
                      ParamKnob::cellWidth, ParamKnob::cellHeight);
        ++i;
    }
}

} // namespace ui
