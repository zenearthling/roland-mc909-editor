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
    setLookAndFeel (&lookAndFeel);
    proc.addEditListener (this);

    addAndMakeVisible (midiOutBox);
    addAndMakeVisible (midiInBox);
    addAndMakeVisible (deviceIdBox);
    addAndMakeVisible (partBox);
    addAndMakeVisible (automateLabel);
    addAndMakeVisible (detectButton);
    addAndMakeVisible (getButton);
    addAndMakeVisible (sendButton);
    addAndMakeVisible (patchNameLabel);
    addAndMakeVisible (patchNameEditor);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (tabs);
    addAndMakeVisible (keyboard);

    automateLabel.setFont (ui::font (10.5f, true).withExtraKerningFactor (0.12f));
    automateLabel.setColour (juce::Label::textColourId, ui::col::textDim);
    automateLabel.setJustificationType (juce::Justification::centredRight);
    automateLabel.setTooltip ("Tones that host automation and Quick SysEx knob moves apply to");

    for (int i = 0; i < 4; ++i)
    {
        auto& b = toneMaskButtons[i];
        b.setButtonText (juce::String (i + 1));
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

    for (int i = 0; i < 4; ++i)
    {
        auto& b = toneButtons[i];
        b.setButtonText ("TONE " + juce::String (i + 1));
        b.setClickingTogglesState (true);
        b.setRadioGroupId (4101);
        b.setColour (juce::TextButton::buttonOnColourId, ui::toneColour (i));
        b.setToggleState (i == proc.getSelectedTone(), juce::dontSendNotification);
        b.onClick = [this, i]
        {
            if (! toneButtons[i].getToggleState())
                return;

            proc.setSelectedTone (i);
            refreshAll();
        };
        addAndMakeVisible (b);
    }

    detectButton.onClick = [this] { proc.detectDevice(); };
    getButton.onClick    = [this] { proc.requestAll(); };
    sendButton.onClick   = [this] { proc.sendAll(); };

    patchNameEditor.setTextToShowWhenEmpty ("(no patch loaded)", juce::Colours::grey);
    patchNameEditor.onReturnKey = [this] { proc.setPatchName (patchNameEditor.getText()); };
    patchNameEditor.onFocusLost = [this] { proc.setPatchName (patchNameEditor.getText()); };

    // Graphical pages first; the generic list pages follow for everything else.
    const auto tabColour = ui::col::bg;
    tabs.setTabBarDepth (32);
    tabs.setOutline (0);
    tabs.setIndent (0);

    pages.add (new TonePage (proc));
    pages.add (new ModPage (proc));
    pages.add (new OutPage (proc));
    tabs.addTab ("Tone", tabColour, pages[0], false);
    tabs.addTab ("Mod",  tabColour, pages[1], false);
    tabs.addTab ("Out",  tabColour, pages[2], false);

    struct TabDef { const char* name; Block block; };
    const TabDef tabDefs[] {
        { "Patch",    Block::patchCommon  },
        { "TMT",      Block::patchTMT     },
        { "Part",     Block::partInfoPart },
        { "Comp/EQ",  Block::compEQ       },
        { "System",   Block::systemCommon },
        { "Master",   Block::mastering    },
        { "All Tone", Block::patchTone    },   // every tone parameter as a plain list
    };

    for (const auto& t : tabDefs)
    {
        auto* panel = new ParamPanel (proc, t.block);
        panels.add (panel);
        tabs.addTab (t.name, tabColour, panel, false);
    }

    keyboard.setAvailableRange (24, 96);
    keyboard.setKeyWidth (17.0f);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffd8dbe1));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff17191e));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff3a3f4a));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colours::white.withAlpha (0.12f));
    keyboard.setColour (juce::MidiKeyboardComponent::textLabelColourId, ui::col::textDim);
    keyboardState.addListener (this);

    statusLabel.setJustificationType (juce::Justification::centredRight);
    statusLabel.setColour (juce::Label::textColourId, ui::col::textDim);
    statusLabel.setFont (ui::font (12.0f));

    updateAccent();

   #ifdef MC909_UI_DEMO
    seedDemo();
   #endif

    refreshAll();

    setResizable (true, true);
    setResizeLimits (1060, 700, 1700, 1200);
    setSize (1120, 760);

    startTimerHz (12);
}

MC909EditorComponent::~MC909EditorComponent()
{
    stopTimer();
    keyboardState.removeListener (this);
    proc.removeEditListener (this);
    setLookAndFeel (nullptr);
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
    for (auto* p : pages)
        p->refresh();

    for (auto* p : panels)
        p->refresh();

    if (! patchNameEditor.hasKeyboardFocus (true))
        patchNameEditor.setText (proc.getPatchName(), juce::dontSendNotification);

    partBox.setSelectedId (proc.getSelectedPart() + 1, juce::dontSendNotification);

    for (int i = 0; i < 4; ++i)
        toneButtons[i].setToggleState (i == proc.getSelectedTone(), juce::dontSendNotification);

    updateAccent();
}

void MC909EditorComponent::updateAccent()
{
    const auto c = ui::toneColour (proc.getSelectedTone());
    setColour (ui::accentColourId, c);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, c.withAlpha (0.55f));
    repaint();
}

#ifdef MC909_UI_DEMO
void MC909EditorComponent::seedDemo()
{
    auto set = [this] (const char* id, int raw) { proc.setParameterValue (ui::findParam (id), raw, false); };

    set ("tone.level", 112);  set ("tone.coarse", 64);  set ("tone.fine", 64);
    set ("tone.rnd_pitch", 4); set ("tone.pan", 64);    set ("tone.pan_kf", 64);
    set ("tone.pitch_kf", 74); set ("tone.rnd_pan", 8); set ("tone.alt_pan", 70);

    set ("tone.wave_group", 3); set ("tone.wave_gain", 1);
    set ("tone.wave_gid", 12); set ("tone.wave_l", 1234); set ("tone.wave_r", 1235);
    set ("tone.fxm_sw", 1);    set ("tone.fxm_color", 2); set ("tone.fxm_depth", 6);

    set ("tone.tvf_type", 1);  set ("tone.tvf_cutoff", 82); set ("tone.tvf_reso", 74);
    set ("tone.tvf_cut_kf", 70); set ("tone.tvf_cut_vsens", 70); set ("tone.tvf_reso_vs", 60);
    set ("tone.tvf_env_depth", 92); set ("tone.tvf_env_vsens", 70);
    set ("tone.tvf_t1_vs", 64); set ("tone.tvf_t4_vs", 60); set ("tone.tvf_t_kf", 66);

    const int pT[4] { 18, 40, 60, 44 }, pL[5] { 64, 92, 64, 40, 64 };
    const int fT[4] { 10, 46, 70, 52 }, fL[5] { 20, 118, 70, 55, 8 };
    const int aT[4] { 6, 32, 64, 56 },  aL[3] { 127, 104, 88 };

    for (int i = 0; i < 4; ++i)
    {
        set (("tone.penv_t" + juce::String (i + 1)).toRawUTF8(), pT[i]);
        set (("tone.tvf_t"  + juce::String (i + 1)).toRawUTF8(), fT[i]);
        set (("tone.tva_t"  + juce::String (i + 1)).toRawUTF8(), aT[i]);
    }
    for (int i = 0; i < 5; ++i)
    {
        set (("tone.penv_l" + juce::String (i)).toRawUTF8(), pL[i]);
        set (("tone.tvf_l"  + juce::String (i)).toRawUTF8(), fL[i]);
    }
    for (int i = 0; i < 3; ++i)
        set (("tone.tva_l" + juce::String (i + 1)).toRawUTF8(), aL[i]);

    set ("tone.penv_depth", 70); set ("tone.penv_vsens", 64);
    set ("tone.tva_vsens", 70);  set ("tone.tva_t1_vs", 64); set ("tone.tva_t4_vs", 60);

    set ("tone.lfo1.rate", 62);  set ("tone.lfo1.delay", 26); set ("tone.lfo1.fade_time", 54);
    set ("tone.lfo1.pitch_dep", 74); set ("tone.lfo1.tvf_dep", 88); set ("tone.lfo1.tva_dep", 64);
    set ("tone.lfo1.pan_dep", 64);  set ("tone.lfo1.offset", 2);
    set ("tone.lfo2.rate", 30);  set ("tone.lfo2.delay", 0);  set ("tone.lfo2.fade_time", 20);
    set ("tone.lfo2.pitch_dep", 64); set ("tone.lfo2.tvf_dep", 64); set ("tone.lfo2.tva_dep", 70);
    set ("tone.lfo2.pan_dep", 80);  set ("tone.lfo2.offset", 2);

    set ("tone.dry_send", 127); set ("tone.rev_send", 34); set ("tone.rx_bender", 1);
    set ("tone.rx_expr", 1);    set ("tone.rx_hold", 1);   set ("tone.bias_level", 64);

    // Lets the screenshot script open any tab / tone.
    tabs.setCurrentTabIndex (juce::SystemStats::getEnvironmentVariable ("MC909_TAB", "0").getIntValue());
    proc.setSelectedTone (juce::SystemStats::getEnvironmentVariable ("MC909_TONE", "0").getIntValue());
}
#endif

void MC909EditorComponent::paint (juce::Graphics& g)
{
    g.fillAll (ui::col::bg);

    // Wordmark
    const auto accent = findColour (ui::accentColourId, true);
    g.setColour (accent);
    g.fillRoundedRectangle (10.0f, 12.0f, 4.0f, 22.0f, 2.0f);

    g.setColour (ui::col::text);
    g.setFont (ui::font (20.0f, true).withExtraKerningFactor (0.04f));
    g.drawText ("MC-909", 22, 8, 100, 30, juce::Justification::centredLeft);

    g.setColour (ui::col::textDim);
    g.setFont (ui::font (11.0f, true).withExtraKerningFactor (0.2f));
    g.drawText ("EDITOR", 98, 8, 70, 30, juce::Justification::centredLeft);
}

void MC909EditorComponent::resized()
{
    auto r = getLocalBounds().reduced (10, 8);

    auto top = r.removeFromTop (30);
    top.removeFromLeft (172);
    midiOutBox.setBounds  (top.removeFromLeft (176).reduced (2, 2));
    midiInBox.setBounds   (top.removeFromLeft (176).reduced (2, 2));
    deviceIdBox.setBounds (top.removeFromLeft (96).reduced (2, 2));
    detectButton.setBounds (top.removeFromLeft (76).reduced (2, 2));

    auto patch = top.removeFromRight (250);
    statusLabel.setBounds (top.reduced (6, 2));
    patchNameLabel.setBounds (patch.removeFromLeft (44).reduced (2, 2));
    patchNameEditor.setBounds (patch.reduced (2, 2));

    r.removeFromTop (6);
    auto second = r.removeFromTop (34);
    partBox.setBounds (second.removeFromLeft (100).reduced (2, 3));
    second.removeFromLeft (10);

    for (auto& b : toneButtons)
    {
        b.setBounds (second.removeFromLeft (88).reduced (2, 2));
        second.removeFromLeft (2);
    }

    auto sendArea = second.removeFromRight (258);
    getButton.setBounds  (sendArea.removeFromLeft (126).reduced (2, 3));
    sendButton.setBounds (sendArea.removeFromLeft (126).reduced (2, 3));

    // Right-aligned "AUTOMATE 1 2 3 4" group, just left of Get/Send.
    second.removeFromRight (14);
    auto leds = second.removeFromRight (40 * 4);
    for (auto& b : toneMaskButtons)
        b.setBounds (leds.removeFromLeft (40).reduced (0, 5));
    automateLabel.setBounds (second.removeFromRight (90).reduced (0, 6));

    r.removeFromTop (6);
    keyboard.setBounds (r.removeFromBottom (68).reduced (0, 2));
    r.removeFromBottom (4);
    tabs.setBounds (r);
}
