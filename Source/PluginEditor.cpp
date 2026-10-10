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
    addAndMakeVisible (probeButton);
    addAndMakeVisible (patchNameLabel);
    addAndMakeVisible (patchNameEditor);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (tabs);
    addAndMakeVisible (keyboard);

    macroBar = std::make_unique<MacroBar> (proc, [this] { teachModeChanged(); });
    addAndMakeVisible (*macroBar);
    teachOverlay = std::make_unique<TeachOverlay> (
        [this] (juce::Point<int> p) { return pickAt (p); },
        [this] (const mc909::ParamDef& d) { macroBar->assignArmed (d); });
    addChildComponent (*teachOverlay);

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
    getButton.onClick    = [this]
    {
        // Alt+click runs the size probe used to diagnose silent patch requests.
        if (juce::ModifierKeys::currentModifiers.isAltDown())
            proc.probePatchSizes();
        else
            proc.requestAll();
    };
    sendButton.onClick   = [this] { proc.sendAll(); };
    probeButton.onClick  = [this] { proc.probePatchSizes(); };

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
    pages.add (new RhythmPage (proc));
    tabs.addTab ("Tone", tabColour, pages[0], false);
    tabs.addTab ("Mod",  tabColour, pages[1], false);
    tabs.addTab ("Out",  tabColour, pages[2], false);
    tabs.addTab ("Rhythm", tabColour, pages[3], false);

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
    statusLabel.setTooltip ("MIDI out port, MIDI in port, and SysEx messages received from the MC-909 so far. "
                            "If RX stays at 0 after Detect/Get, nothing is coming back. "
                            "A log of every SysEx message is written to Documents\\MC909-Editor-midi-log.txt");

    updateAccent();

   #ifdef MC909_UI_DEMO
    seedDemo();
   #endif

    refreshAll();

    setResizable (true, true);
    setResizeLimits (1060, 746, 1700, 1250);
    setSize (1120, 806);

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
    deviceSeen = true;
}

void MC909EditorComponent::timerCallback()
{
    if (needsRefresh)
    {
        needsRefresh = false;
        refreshAll();
    }

    auto& hub = proc.midi();
    const int pending = hub.pendingCount();
    const bool portsOk = hub.isConnected() && hub.isInputOpen();

    juce::String text;
    if (pending > 0)
        text = "Sending... " + juce::String (pending);
    else
        text << (deviceSeen ? "909 found  " : "")
             << "Out " << (hub.isConnected() ? "ok" : "CLOSED")
             << "  In " << (hub.isInputOpen() ? "ok" : "CLOSED")
             << "  RX " << hub.rxSysExCount()
             << " (" << proc.rxStored() << " stored)";

    const auto colour = portsOk ? ui::col::textDim : juce::Colour (0xffff6f6f);
    if (statusLabel.findColour (juce::Label::textColourId) != colour)
        statusLabel.setColour (juce::Label::textColourId, colour);

    if (statusLabel.getText() != text)
        statusLabel.setText (text, juce::dontSendNotification);
}

void MC909EditorComponent::refreshAll()
{
    if (macroBar != nullptr) macroBar->refresh();
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
    if (juce::SystemStats::getEnvironmentVariable ("MC909_TEACH", "0").getIntValue() != 0)
    {
        proc.assignMacro (0, &ui::findParam ("rn.tvf_cutoff"));
        proc.assignMacro (1, &ui::findParam ("rn.tvf_reso"));
        proc.assignMacro (2, &ui::findParam ("rn.pan"));
        macroBar->assignArmed (ui::findParam ("rn.fine"));
        macroBar->assignArmed (ui::findParam ("rn.fine"));
        macroBar->assignArmed (ui::findParam ("rn.fine"));
        macroBar->assignArmed (ui::findParam ("rn.fine"));
        macroBar->assignArmed (ui::findParam ("rn.fine"));
        macroBar->assignArmed (ui::findParam ("rn.fine"));
    }
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

    auto sendArea = second.removeFromRight (336);
    probeButton.setBounds (sendArea.removeFromLeft (76).reduced (2, 3));
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
    macroBar->setBounds (r.removeFromBottom (42));
    tabs.setBounds (r);
    teachOverlay->setBounds (r.withTrimmedTop (32));
}

//==============================================================================
void MacroBar::Cell::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const auto* d = owner->proc.getMacroDef (slot);
    const bool isArmed = owner->isTeaching() && owner->armedSlot() == slot;
    const auto accent = findColour (ui::accentColourId, true);

    g.setColour (isArmed ? accent.withAlpha (0.25f) : ui::col::card);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (isArmed ? accent : ui::col::cardEdge);
    g.drawRoundedRectangle (r, 5.0f, isArmed ? 2.0f : 1.0f);

    auto t = getLocalBounds().reduced (8, 3);
    t.removeFromRight (34);
    g.setColour (accent);
    g.setFont (ui::font (11.0f, true));
    g.drawText (juce::String (slot + 1), t.removeFromLeft (14), juce::Justification::centredLeft);
    g.setColour (d != nullptr ? ui::col::text : ui::col::textDim);
    g.setFont (ui::font (12.0f, d != nullptr));
    g.drawFittedText (d != nullptr ? d->name : (isArmed ? juce::String ("click a control...") : juce::String ("-")),
                      t, juce::Justification::centredLeft, 1);
}

void MacroBar::Cell::mouseDown (const juce::MouseEvent&)
{
    if (owner->isTeaching())
        owner->setArmed (slot);
}

void MacroBar::Cell::mouseDoubleClick (const juce::MouseEvent&)
{
    owner->proc.assignMacro (slot, nullptr);
}

MacroBar::MacroBar (MC909EditorProcessor& p, std::function<void()> onChanged)
    : proc (p), changed (std::move (onChanged))
{
    teachButton.setClickingTogglesState (true);
    teachButton.setTooltip ("Pick a knob, then click any control to assign it. In Live: Configure mode, click the plugin's Knob, turn the MPK knob.");
    teachButton.onClick = [this]
    {
        armed = 0;
        refresh();
        changed();
    };
    clearButton.onClick = [this]
    {
        for (int i = 0; i < MC909EditorProcessor::numMacros; ++i)
            proc.assignMacro (i, nullptr);
    };
    addAndMakeVisible (teachButton);
    addAndMakeVisible (clearButton);

    hint.setFont (ui::font (11.0f));
    hint.setColour (juce::Label::textColourId, ui::col::textDim);
    addAndMakeVisible (hint);

    for (int i = 0; i < 8; ++i)
    {
        cells[i].owner = this;
        cells[i].slot = i;
        cells[i].knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        cells[i].knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        cells[i].knob.setTooltip ("Knob " + juce::String (i + 1) + ": the one to MIDI-map in Live");
        if (auto* prm = proc.getMacroParameter (i))
            cells[i].attach = std::make_unique<juce::SliderParameterAttachment> (*prm, cells[i].knob);
        cells[i].addAndMakeVisible (cells[i].knob);
        addAndMakeVisible (cells[i]);
    }
    refresh();
}

void MacroBar::resized()
{
    auto r = getLocalBounds().reduced (0, 3);
    teachButton.setBounds (r.removeFromLeft (110).reduced (2, 3));
    clearButton.setBounds (r.removeFromRight (80).reduced (2, 3));
    auto h = r.removeFromRight (0);
    juce::ignoreUnused (h);
    const int w = r.getWidth() / 8;
    for (auto& c : cells)
        c.setBounds (r.removeFromLeft (w));
}

void MacroBar::paint (juce::Graphics&) {}

void MacroBar::setArmed (int slot)
{
    armed = juce::jlimit (0, 7, slot);
    refresh();
}

void MacroBar::stopTeaching()
{
    teachButton.setToggleState (false, juce::dontSendNotification);
    refresh();
}

void MacroBar::assignArmed (const mc909::ParamDef& d)
{
    proc.assignMacro (armed, &d);
    armed = (armed + 1) % 8;
    refresh();
}

void MacroBar::refresh()
{
    teachButton.setButtonText (isTeaching() ? "Done teaching" : "Teach knobs");
    for (auto& c : cells)
        c.repaint();
}

//==============================================================================
void TeachOverlay::paint (juce::Graphics& g)
{
    const auto accent = findColour (ui::accentColourId, true);
    g.fillAll (juce::Colours::black.withAlpha (0.18f));
    g.setColour (accent.withAlpha (0.7f));
    g.drawRect (getLocalBounds(), 2);

    if (hover != nullptr)
    {
        g.setFont (ui::font (13.0f, true));
        const auto txt = "Assign: " + hover->name;
        const int w = g.getCurrentFont().getStringWidth (txt) + 16;
        auto box = juce::Rectangle<int> (w, 22).withCentre (pos.translated (0, -26));
        box = box.constrainedWithin (getLocalBounds());
        g.setColour (accent);
        g.fillRoundedRectangle (box.toFloat(), 4.0f);
        g.setColour (juce::Colours::black);
        g.drawText (txt, box, juce::Justification::centred);
    }
}

void TeachOverlay::mouseMove (const juce::MouseEvent& e)
{
    pos = e.getPosition();
    hover = pickFn (pos);
    repaint();
}

void TeachOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (auto* d = pickFn (e.getPosition()))
        assignFn (*d);
    repaint();
}

const ParamDef* MC909EditorComponent::pickAt (juce::Point<int> inOverlay)
{
    auto* content = tabs.getCurrentContentComponent();
    if (content == nullptr)
        return nullptr;

    const auto pt = content->getLocalPoint (teachOverlay.get(), inOverlay);
    auto* hit = content->getComponentAt (pt);

    for (auto* c = hit; c != nullptr && c != content->getParentComponent(); c = c->getParentComponent())
    {
        if (auto* pc = dynamic_cast<ui::ParamControl*> (c))
            return pc->teachDef (pc->getLocalPoint (content, pt));
        if (auto* row = dynamic_cast<ParamRow*> (c))
            return &row->definition();
    }
    return nullptr;
}

void MC909EditorComponent::teachModeChanged()
{
    const bool on = macroBar->isTeaching();
    teachOverlay->setVisible (on);
    if (on)
        teachOverlay->toFront (false);
    resized();
}
