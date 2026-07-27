#include "PluginEditor.h"

using namespace mc909;

//==============================================================================
ParamRow::ParamRow (MC909EditorProcessor& p, const ParamDef& d)
    : proc (p), def (d)
{
    label.setText (def.name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);

    if (def.isChoice())
    {
        for (int i = 0; i < def.choices.size(); ++i)
            combo.addItem (def.choices[i], i + 1);

        combo.addListener (this);
        addAndMakeVisible (combo);
    }
    else
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 20);
        slider.setRange (def.toDisplay (def.rawMin), def.toDisplay (def.rawMax),
                         juce::jmax (1, def.dispMul));
        slider.addListener (this);
        addAndMakeVisible (slider);
    }

    refresh();
}

void ParamRow::resized()
{
    auto r = getLocalBounds().reduced (2, 1);
    label.setBounds (r.removeFromLeft (160));

    if (def.isChoice()) combo.setBounds (r.removeFromLeft (200));
    else                slider.setBounds (r);
}

void ParamRow::refresh()
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    const int raw = proc.getParameterValue (def);

    if (def.isChoice())
        combo.setSelectedId (juce::jlimit (1, def.choices.size(), raw - def.rawMin + 1),
                             juce::dontSendNotification);
    else
        slider.setValue (def.toDisplay (raw), juce::dontSendNotification);
}

void ParamRow::sliderValueChanged (juce::Slider* s)
{
    if (updating)
        return;

    proc.setParameterValue (def, def.fromDisplay ((int) s->getValue()));
}

void ParamRow::comboBoxChanged (juce::ComboBox* c)
{
    if (updating)
        return;

    proc.setParameterValue (def, def.rawMin + c->getSelectedId() - 1);
}

//==============================================================================
ParamPanel::ParamPanel (MC909EditorProcessor& p, Block b)
{
    for (const auto* def : paramsForBlock (b))
        rows.add (new ParamRow (p, *def));

    for (auto* r : rows)
        holder.addAndMakeVisible (r);

    viewport.setViewedComponent (&holder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void ParamPanel::resized()
{
    viewport.setBounds (getLocalBounds());

    constexpr int rowHeight = 24;
    const int width = juce::jmax (400, viewport.getMaximumVisibleWidth());
    holder.setSize (width, rowHeight * rows.size());

    int y = 0;
    for (auto* r : rows)
    {
        r->setBounds (0, y, width, rowHeight);
        y += rowHeight;
    }
}

void ParamPanel::refresh()
{
    for (auto* r : rows)
        r->refresh();
}

//==============================================================================
MC909EditorComponent::MC909EditorComponent (MC909EditorProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    proc.addEditListener (this);

    addAndMakeVisible (midiOutBox);
    addAndMakeVisible (midiInBox);
    addAndMakeVisible (deviceIdBox);
    addAndMakeVisible (partBox);
    addAndMakeVisible (toneBox);
    addAndMakeVisible (detectButton);
    addAndMakeVisible (getButton);
    addAndMakeVisible (sendButton);
    addAndMakeVisible (patchNameLabel);
    addAndMakeVisible (patchNameEditor);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (tabs);
    addAndMakeVisible (keyboard);

    for (int i = 0; i < 4; ++i)
    {
        auto& b = toneMaskButtons[i];
        b.setButtonText ("T" + juce::String (i + 1));
        b.setToggleState ((proc.getToneMask() & (1 << i)) != 0, juce::dontSendNotification);
        b.onClick = [this]
        {
            int mask = 0;
            for (int n = 0; n < 4; ++n)
                if (toneMaskButtons[n].getToggleState())
                    mask |= (1 << n);

            proc.setToneMask (mask);
        };
        addAndMakeVisible (b);
    }

    refreshDeviceLists();

    midiOutBox.onChange = [this]
    {
        const auto devices = proc.midi().availableOutputs();
        const int idx = midiOutBox.getSelectedId() - 1;
        proc.midi().openOutput (juce::isPositiveAndBelow (idx, devices.size())
                                    ? devices[idx].identifier : juce::String());
    };

    midiInBox.onChange = [this]
    {
        const auto devices = proc.midi().availableInputs();
        const int idx = midiInBox.getSelectedId() - 1;
        proc.midi().openInput (juce::isPositiveAndBelow (idx, devices.size())
                                   ? devices[idx].identifier : juce::String());
    };

    for (int i = 0x10; i <= 0x1F; ++i)
        deviceIdBox.addItem ("Unit " + juce::String (i - 0x0F), i);
    deviceIdBox.addItem ("Broadcast", 0x7F);
    deviceIdBox.setSelectedId (proc.getDeviceId(), juce::dontSendNotification);
    deviceIdBox.onChange = [this] { proc.setDeviceId (deviceIdBox.getSelectedId()); };

    for (int i = 1; i <= 16; ++i)
        partBox.addItem ("Part " + juce::String (i), i);
    partBox.setSelectedId (proc.getSelectedPart() + 1, juce::dontSendNotification);
    partBox.onChange = [this]
    {
        proc.setSelectedPart (partBox.getSelectedId() - 1);
        proc.requestAll();
    };

    for (int i = 1; i <= 4; ++i)
        toneBox.addItem ("Tone " + juce::String (i), i);
    toneBox.setSelectedId (proc.getSelectedTone() + 1, juce::dontSendNotification);
    toneBox.onChange = [this] { proc.setSelectedTone (toneBox.getSelectedId() - 1); refreshAll(); };

    detectButton.onClick = [this] { proc.detectDevice(); };
    getButton.onClick    = [this] { proc.requestAll(); };
    sendButton.onClick   = [this] { proc.sendAll(); };

    patchNameEditor.setTextToShowWhenEmpty ("(no patch loaded)", juce::Colours::grey);
    patchNameEditor.onReturnKey = [this] { proc.setPatchName (patchNameEditor.getText()); };
    patchNameEditor.onFocusLost = [this] { proc.setPatchName (patchNameEditor.getText()); };

    struct TabDef { const char* name; Block block; };
    const TabDef tabDefs[] {
        { "Tone",     Block::patchTone    },
        { "Patch",    Block::patchCommon  },
        { "TMT",      Block::patchTMT     },
        { "Part",     Block::partInfoPart },
        { "Comp/EQ",  Block::compEQ       },
        { "System",   Block::systemCommon },
        { "Master",   Block::mastering    },
    };

    for (const auto& t : tabDefs)
    {
        auto* panel = new ParamPanel (proc, t.block);
        panels.add (panel);
        tabs.addTab (t.name, juce::Colours::darkgrey.darker(), panel, false);
    }

    keyboard.setAvailableRange (24, 96);
    keyboardState.addListener (this);

    statusLabel.setJustificationType (juce::Justification::centredRight);

    setResizable (true, true);
    setResizeLimits (760, 520, 1600, 1200);
    setSize (900, 680);

    startTimerHz (12);
}

MC909EditorComponent::~MC909EditorComponent()
{
    stopTimer();
    keyboardState.removeListener (this);
    proc.removeEditListener (this);
}

void MC909EditorComponent::handleNoteOn (juce::MidiKeyboardState*, int, int note, float velocity)
{
    proc.playTestNote (note, juce::jlimit (1, 127, (int) (velocity * 127.0f)), true);
}

void MC909EditorComponent::handleNoteOff (juce::MidiKeyboardState*, int, int note, float)
{
    proc.playTestNote (note, 0, false);
}

void MC909EditorComponent::refreshDeviceLists()
{
    midiOutBox.clear (juce::dontSendNotification);
    midiInBox.clear (juce::dontSendNotification);

    const auto outs = proc.midi().availableOutputs();
    for (int i = 0; i < outs.size(); ++i)
        midiOutBox.addItem (outs[i].name, i + 1);

    const auto ins = proc.midi().availableInputs();
    for (int i = 0; i < ins.size(); ++i)
        midiInBox.addItem (ins[i].name, i + 1);

    midiOutBox.setTextWhenNothingSelected ("MIDI Out");
    midiInBox.setTextWhenNothingSelected ("MIDI In");

    for (int i = 0; i < outs.size(); ++i)
        if (outs[i].identifier == proc.midi().currentOutputId())
            midiOutBox.setSelectedId (i + 1, juce::dontSendNotification);

    for (int i = 0; i < ins.size(); ++i)
        if (ins[i].identifier == proc.midi().currentInputId())
            midiInBox.setSelectedId (i + 1, juce::dontSendNotification);
}

void MC909EditorComponent::modelChanged()
{
    needsRefresh = true;
}

void MC909EditorComponent::deviceDetected (const juce::String&)
{
    statusLabel.setText ("MC-909 detected", juce::dontSendNotification);
}

void MC909EditorComponent::timerCallback()
{
    if (needsRefresh)
    {
        needsRefresh = false;
        refreshAll();
    }

    const int pending = proc.midi().pendingCount();
    if (pending > 0)
        statusLabel.setText ("Sending... " + juce::String (pending), juce::dontSendNotification);
    else if (statusLabel.getText().startsWith ("Sending"))
        statusLabel.setText (proc.midi().isConnected() ? "Ready" : "No MIDI output",
                             juce::dontSendNotification);
}

void MC909EditorComponent::refreshAll()
{
    for (auto* p : panels)
        p->refresh();

    if (! patchNameEditor.hasKeyboardFocus (true))
        patchNameEditor.setText (proc.getPatchName(), juce::dontSendNotification);

    partBox.setSelectedId (proc.getSelectedPart() + 1, juce::dontSendNotification);
    toneBox.setSelectedId (proc.getSelectedTone() + 1, juce::dontSendNotification);
}

void MC909EditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff23252a));
}

void MC909EditorComponent::resized()
{
    auto r = getLocalBounds().reduced (6);

    auto top = r.removeFromTop (28);
    midiOutBox.setBounds (top.removeFromLeft (180).reduced (2));
    midiInBox.setBounds  (top.removeFromLeft (180).reduced (2));
    deviceIdBox.setBounds (top.removeFromLeft (110).reduced (2));
    detectButton.setBounds (top.removeFromLeft (80).reduced (2));
    statusLabel.setBounds (top.reduced (2));

    auto second = r.removeFromTop (28);
    partBox.setBounds (second.removeFromLeft (110).reduced (2));
    toneBox.setBounds (second.removeFromLeft (100).reduced (2));
    for (auto& b : toneMaskButtons)
        b.setBounds (second.removeFromLeft (46).reduced (2));
    getButton.setBounds (second.removeFromLeft (130).reduced (2));
    sendButton.setBounds (second.removeFromLeft (130).reduced (2));

    auto third = r.removeFromTop (28);
    patchNameLabel.setBounds (third.removeFromLeft (50).reduced (2));
    patchNameEditor.setBounds (third.removeFromLeft (200).reduced (2));

    keyboard.setBounds (r.removeFromBottom (70).reduced (2));
    tabs.setBounds (r.reduced (2));
}
