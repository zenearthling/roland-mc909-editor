#include "Pages.h"

using namespace ui;

namespace
{
    /** Split `area` into n columns with a gap, weights are relative widths. */
    std::vector<juce::Rectangle<int>> splitColumns (juce::Rectangle<int> area, std::initializer_list<float> weights, int gap)
    {
        float total = 0.0f;
        for (float w : weights) total += w;

        const int usable = area.getWidth() - gap * ((int) weights.size() - 1);
        std::vector<juce::Rectangle<int>> out;
        int x = area.getX();
        int i = 0;

        for (float w : weights)
        {
            const bool last = (++i == (int) weights.size());
            const int width = last ? area.getRight() - x : juce::roundToInt ((float) usable * w / total);
            out.push_back ({ x, area.getY(), width, area.getHeight() });
            x += width + gap;
        }
        return out;
    }
}

//==============================================================================
TonePage::TonePage (MC909EditorProcessor& p) : PageBase (p)
{
    waveGroup = add<ParamChoice> ("tone.wave_group", "Wave Group");
    waveGain  = add<ParamChoice> ("tone.wave_gain", "Gain");
    waveGid   = add<ParamNumber> ("tone.wave_gid", "Group ID");
    waveL     = add<ParamNumber> ("tone.wave_l", "Wave L");
    waveR     = add<ParamNumber> ("tone.wave_r", "Wave R");
    fxmSwitch = add<ParamToggle> ("tone.fxm_sw", "FXM");
    fxmColor  = add<ParamChoice> ("tone.fxm_color", "FXM Color");
    fxmDepth  = add<ParamKnob>   ("tone.fxm_depth", "FXM Depth");

    coarse   = add<ParamKnob> ("tone.coarse", "Coarse");
    fine     = add<ParamKnob> ("tone.fine", "Fine");
    rndPitch = add<ParamKnob> ("tone.rnd_pitch", "Random");
    pitchKf  = add<ParamKnob> ("tone.pitch_kf", "Key Follow");

    curve      = add<FilterCurve> ();
    filterType = add<ParamChoice> ("tone.tvf_type", "Type", true);   // sits on top of the curve
    cutoff     = add<ParamKnob> ("tone.tvf_cutoff", "Cutoff");
    reso       = add<ParamKnob> ("tone.tvf_reso", "Resonance");
    cutKf      = add<ParamKnob> ("tone.tvf_cut_kf", "Key Follow");
    cutVel     = add<ParamKnob> ("tone.tvf_cut_vsens", "Cut Vel");
    resoVel    = add<ParamKnob> ("tone.tvf_reso_vs", "Reso Vel");

    level  = add<ParamKnob> ("tone.level", "Level");
    pan    = add<ParamKnob> ("tone.pan", "Pan");
    panKf  = add<ParamKnob> ("tone.pan_kf", "Pan KF");
    rndPan = add<ParamKnob> ("tone.rnd_pan", "Rnd Pan");
    altPan = add<ParamKnob> ("tone.alt_pan", "Alt Pan");

    pitchEnv  = add<EnvelopeEditor> (EnvelopeEditor::Kind::pitch);
    filterEnv = add<EnvelopeEditor> (EnvelopeEditor::Kind::filter);
    ampEnv    = add<EnvelopeEditor> (EnvelopeEditor::Kind::amp);

    pDepth = add<ParamKnob> ("tone.penv_depth", "Depth");
    pVel   = add<ParamKnob> ("tone.penv_vsens", "Vel Sens");
    pT1    = add<ParamKnob> ("tone.penv_t1_vs", "T1 Vel");
    pT4    = add<ParamKnob> ("tone.penv_t4_vs", "T4 Vel");
    pKf    = add<ParamKnob> ("tone.penv_t_kf", "Time KF");

    fDepth = add<ParamKnob> ("tone.tvf_env_depth", "Depth");
    fVel   = add<ParamKnob> ("tone.tvf_env_vsens", "Vel Sens");
    fT1    = add<ParamKnob> ("tone.tvf_t1_vs", "T1 Vel");
    fT4    = add<ParamKnob> ("tone.tvf_t4_vs", "T4 Vel");
    fKf    = add<ParamKnob> ("tone.tvf_t_kf", "Time KF");

    aVel = add<ParamKnob> ("tone.tva_vsens", "Vel Sens");
    aT1  = add<ParamKnob> ("tone.tva_t1_vs", "T1 Vel");
    aT4  = add<ParamKnob> ("tone.tva_t4_vs", "T4 Vel");
    aKf  = add<ParamKnob> ("tone.tva_t_kf", "Time KF");
}

void TonePage::resized()
{
    cards.clear();

    auto r = getLocalBounds().reduced (6);
    constexpr int gap = 8;

    auto top = r.removeFromTop (juce::jmin (232, r.getHeight() / 2));
    r.removeFromTop (gap);

    //-- top row ---------------------------------------------------------------
    const auto cols = splitColumns (top, { 0.27f, 0.17f, 0.33f, 0.23f }, gap);

    {   // WAVE
        cards.push_back ({ cols[0], "Wave" });
        auto c = contentOf (cards.back());

        auto row = c.removeFromTop (40);
        const int half = (row.getWidth() - 8) / 2;
        waveGroup->setBounds (row.removeFromLeft (half));
        row.removeFromLeft (8);
        waveGain->setBounds (row);

        c.removeFromTop (6);
        auto nums = c.removeFromTop (40);
        const int third = (nums.getWidth() - 12) / 3;
        waveGid->setBounds (nums.removeFromLeft (third));
        nums.removeFromLeft (6);
        waveL->setBounds (nums.removeFromLeft (third));
        nums.removeFromLeft (6);
        waveR->setBounds (nums);

        c.removeFromTop (6);
        fxmSwitch->setBounds (c.getX(), c.getY(), 62, 40);
        fxmColor->setBounds (c.getX() + 70, c.getY(), 62, 40);
        fxmDepth->setBounds (c.getX() + 140, c.getY() - 4, ParamKnob::cellWidth, ParamKnob::cellHeight);
    }

    {   // PITCH
        cards.push_back ({ cols[1], "Pitch" });
        placeKnobs (contentOf (cards.back()), { coarse, fine, rndPitch, pitchKf }, 2);
    }

    {   // FILTER
        cards.push_back ({ cols[2], "Filter" });
        auto c = contentOf (cards.back());

        auto scope = c.removeFromTop (juce::jmax (60, c.getHeight() - ParamKnob::cellHeight - 6));
        curve->setBounds (scope);
        filterType->setBounds (scope.getX() + 6, scope.getY() + 6, 96, 24);

        c.removeFromTop (6);
        placeKnobs (c, { cutoff, reso, cutKf, cutVel, resoVel }, 5);
    }

    {   // AMP
        cards.push_back ({ cols[3], "Amp" });
        placeKnobs (contentOf (cards.back()), { level, pan, panKf, rndPan, altPan }, 3);
    }

    //-- envelope row ---------------------------------------------------------
    const auto envCols = splitColumns (r, { 1.0f, 1.0f, 1.0f }, gap);

    auto layoutEnv = [this] (juce::Rectangle<int> area, const char* title, EnvelopeEditor* env,
                             std::initializer_list<juce::Component*> knobs)
    {
        cards.push_back ({ area, title });
        auto c = contentOf (cards.back());

        auto envArea = c.removeFromTop (juce::jmax (60, c.getHeight() - ParamKnob::cellHeight - 6));
        env->setBounds (envArea);
        c.removeFromTop (6);
        placeKnobs (c, knobs, 5);
    };

    layoutEnv (envCols[0], "Pitch Envelope",  pitchEnv,  { pDepth, pVel, pT1, pT4, pKf });
    layoutEnv (envCols[1], "Filter Envelope", filterEnv, { fDepth, fVel, fT1, fT4, fKf });
    layoutEnv (envCols[2], "Amp Envelope",    ampEnv,    { aVel, aT1, aT4, aKf });

    repaint();
}

//==============================================================================
ModPage::ModPage (MC909EditorProcessor& p) : PageBase (p)
{
    for (int n = 0; n < 2; ++n)
    {
        const juce::String pre = "tone.lfo" + juce::String (n + 1) + ".";
        auto& l = lfo[n];

        l.scope    = add<LfoScope> (n + 1);
        l.offset   = add<ParamChoice> (pre + "offset", "Offset");
        l.fadeMode = add<ParamChoice> (pre + "fade_mode", "Fade Mode");
        l.keyTrig  = add<ParamToggle> (pre + "key_trig", "Key Trigger");

        l.rate     = add<ParamKnob> (pre + "rate", "Rate");
        l.detune   = add<ParamKnob> (pre + "detune", "Detune");
        l.wave     = add<ParamKnob> (pre + "morph", "Waveform");
        l.delay    = add<ParamKnob> (pre + "delay", "Delay");
        l.delayKf  = add<ParamKnob> (pre + "delay_kf", "Delay KF");
        l.fadeTime = add<ParamKnob> (pre + "fade_time", "Fade Time");
        l.pitchDep = add<ParamKnob> (pre + "pitch_dep", "Pitch");
        l.tvfDep   = add<ParamKnob> (pre + "tvf_dep", "Filter");
        l.tvaDep   = add<ParamKnob> (pre + "tva_dep", "Amp");
        l.panDep   = add<ParamKnob> (pre + "pan_dep", "Pan");
    }
}

void ModPage::resized()
{
    cards.clear();

    auto r = getLocalBounds().reduced (6);
    const auto cols = splitColumns (r, { 1.0f, 1.0f }, 8);

    for (int n = 0; n < 2; ++n)
    {
        auto& l = lfo[n];
        cards.push_back ({ cols[(size_t) n].withHeight (juce::jmin (cols[(size_t) n].getHeight(), 410)),
                           "LFO " + juce::String (n + 1) });
        auto c = contentOf (cards.back());

        l.scope->setBounds (c.removeFromTop (92));
        c.removeFromTop (8);

        auto row = c.removeFromTop (40);
        l.offset->setBounds (row.removeFromLeft (120));
        row.removeFromLeft (8);
        l.fadeMode->setBounds (row.removeFromLeft (120));
        row.removeFromLeft (8);
        l.keyTrig->setBounds (row.removeFromLeft (84));

        c.removeFromTop (8);
        placeKnobs (c, { l.rate, l.detune, l.wave, l.delay, l.delayKf }, 5);
        c.removeFromTop (ParamKnob::cellHeight);
        placeKnobs (c, { l.fadeTime, l.pitchDep, l.tvfDep, l.tvaDep, l.panDep }, 5);
    }

    repaint();
}

//==============================================================================
OutPage::OutPage (MC909EditorProcessor& p) : PageBase (p)
{
    dry    = add<ParamKnob> ("tone.dry_send", "Dry");
    revMfx = add<ParamKnob> ("tone.rev_send_mfx", "Rev (MFX)");
    rev    = add<ParamKnob> ("tone.rev_send", "Reverb");

    envMode   = add<ParamChoice> ("tone.env_mode", "Env Mode");
    delayMode = add<ParamChoice> ("tone.delay_mode", "Delay Mode");
    delayTime = add<ParamKnob>   ("tone.delay_time", "Delay Time");
    tempoSync = add<ParamToggle> ("tone.tempo_sync", "Tempo Sync");
    redamper  = add<ParamToggle> ("tone.redamper", "Redamper");

    rxBender = add<ParamToggle> ("tone.rx_bender", "Bender");
    rxExpr   = add<ParamToggle> ("tone.rx_expr", "Expression");
    rxHold   = add<ParamToggle> ("tone.rx_hold", "Hold-1");
    panMode  = add<ParamChoice> ("tone.rx_pan_mode", "Pan Mode");

    biasLevel = add<ParamKnob>   ("tone.bias_level", "Level");
    biasPos   = add<ParamKnob>   ("tone.bias_pos", "Position");
    biasDir   = add<ParamChoice> ("tone.bias_dir", "Direction");

    for (int c = 0; c < 4; ++c)
        for (int s = 0; s < 4; ++s)
            ctrl[c][s] = add<ParamChoice> ("tone.ctrl" + juce::String (c + 1) + "_sw" + juce::String (s + 1),
                                           juce::String(), true);

    for (int i = 0; i < 4; ++i)
    {
        rowLabels[i].setText ("Control " + juce::String (i + 1), juce::dontSendNotification);
        colLabels[i].setText ("Switch " + juce::String (i + 1), juce::dontSendNotification);

        for (auto* l : { &rowLabels[i], &colLabels[i] })
        {
            l->setFont (font (11.0f));
            l->setColour (juce::Label::textColourId, col::textDim);
            l->setInterceptsMouseClicks (false, false);
            addAndMakeVisible (*l);
        }

        colLabels[i].setJustificationType (juce::Justification::centredLeft);
    }
}

void OutPage::resized()
{
    cards.clear();

    auto r = getLocalBounds().reduced (6);
    constexpr int gap = 8;

    auto top = r.removeFromTop (juce::jmin (184, r.getHeight() / 2));
    r.removeFromTop (gap);
    const auto cols = splitColumns (top, { 1.0f, 1.2f, 1.0f, 0.9f }, gap);

    cards.push_back ({ cols[0], "Sends" });
    placeKnobs (contentOf (cards.back()), { dry, revMfx, rev }, 3);

    {
        cards.push_back ({ cols[1], "Playback" });
        auto c = contentOf (cards.back());

        auto row = c.removeFromTop (40);
        const int half = (row.getWidth() - 8) / 2;
        envMode->setBounds (row.removeFromLeft (half));
        row.removeFromLeft (8);
        delayMode->setBounds (row);

        c.removeFromTop (8);
        delayTime->setBounds (c.getX(), c.getY(), ParamKnob::cellWidth, ParamKnob::cellHeight);
        tempoSync->setBounds (c.getX() + 68, c.getY() + 8, 72, 40);
        redamper->setBounds (c.getX() + 148, c.getY() + 8, 72, 40);
    }

    {
        cards.push_back ({ cols[2], "Receive" });
        auto c = contentOf (cards.back());

        auto row = c.removeFromTop (40);
        const int third = (row.getWidth() - 12) / 3;
        rxBender->setBounds (row.removeFromLeft (third));
        row.removeFromLeft (6);
        rxExpr->setBounds (row.removeFromLeft (third));
        row.removeFromLeft (6);
        rxHold->setBounds (row);

        c.removeFromTop (8);
        panMode->setBounds (c.removeFromTop (40).removeFromLeft (150));
    }

    {
        cards.push_back ({ cols[3], "Bias" });
        auto c = contentOf (cards.back());

        placeKnobs (c.removeFromTop (ParamKnob::cellHeight), { biasLevel, biasPos }, 2);
        c.removeFromTop (8);
        biasDir->setBounds (c.removeFromTop (40).removeFromLeft (150));
    }

    {
        cards.push_back ({ r.withHeight (juce::jmin (r.getHeight(), 196)), "Tone Control Switches" });
        auto c = contentOf (cards.back());

        constexpr int labelW = 74, cellW = 112, rowH = 30;

        auto header = c.removeFromTop (18);
        header.removeFromLeft (labelW);
        for (int s = 0; s < 4; ++s)
            colLabels[s].setBounds (header.removeFromLeft (cellW + 8));

        for (int cRow = 0; cRow < 4; ++cRow)
        {
            auto row = c.removeFromTop (rowH + 4);
            rowLabels[cRow].setBounds (row.removeFromLeft (labelW).withHeight (rowH));

            for (int s = 0; s < 4; ++s)
            {
                ctrl[cRow][s]->setBounds (row.removeFromLeft (cellW).withHeight (rowH - 4));
                row.removeFromLeft (8);
            }
        }
    }

    repaint();
}


//==============================================================================
PatchPage::PatchPage (MC909EditorProcessor& p) : PageBase (p)
{
    level  = add<ParamKnob> ("patch.level", "Level");
    pan    = add<ParamKnob> ("patch.pan", "Pan");
    coarse = add<ParamKnob> ("patch.coarse", "Coarse");
    fine   = add<ParamKnob> ("patch.fine", "Fine");
    octave = add<ParamKnob> ("patch.octave", "Octave");
    analog = add<ParamKnob> ("patch.analog_feel", "Analog Feel");
    stretch = add<ParamChoice> ("patch.stretch", "Stretch Tune");

    monoPoly = add<ParamChoice> ("patch.mono_poly", "Mono / Poly");
    priority = add<ParamChoice> ("patch.priority", "Priority");
    legato   = add<ParamToggle> ("patch.legato", "Legato");
    retrig   = add<ParamToggle> ("patch.legato_retrig", "Retrigger");
    bendUp   = add<ParamKnob>   ("patch.bend_up", "Bend Up");
    bendDown = add<ParamKnob>   ("patch.bend_down", "Bend Down");

    portaSw    = add<ParamToggle> ("patch.porta_sw", "Portamento");
    portaMode  = add<ParamChoice> ("patch.porta_mode", "Mode");
    portaType  = add<ParamChoice> ("patch.porta_type", "Type");
    portaStart = add<ParamChoice> ("patch.porta_start", "Start");
    portaTime  = add<ParamKnob>   ("patch.porta_time", "Time");

    cutOfs  = add<ParamKnob> ("patch.cutoff_ofs", "Cutoff");
    resoOfs = add<ParamKnob> ("patch.reso_ofs", "Resonance");
    attOfs  = add<ParamKnob> ("patch.attack_ofs", "Attack");
    relOfs  = add<ParamKnob> ("patch.release_ofs", "Release");
    velOfs  = add<ParamKnob> ("patch.velsens_ofs", "Vel Sens");
    unison    = add<ParamToggle> ("patch.unison_sw", "Unison");
    unisonFat = add<ParamKnob>   ("patch.unison_fat", "Fat");
    tmtCtrl   = add<ParamToggle> ("patch.tmt_ctrl", "TMT Control");

    for (int n = 0; n < 4; ++n)
    {
        const juce::String tag ("patch.mtx" + juce::String (n + 1));
        mtx[n].src = add<ParamChoice> (tag + ".src", juce::String(), true);

        for (int d = 0; d < 4; ++d)
        {
            mtx[n].dst[d]  = add<ParamChoice> (tag + ".dst" + juce::String (d + 1), juce::String(), true);
            mtx[n].sens[d] = add<ParamNumber> (tag + ".sens" + juce::String (d + 1), " ");
        }

        mtx[n].label.setText ("Matrix " + juce::String (n + 1), juce::dontSendNotification);
        mtx[n].label.setFont (font (11.0f));
        mtx[n].label.setColour (juce::Label::textColourId, col::textDim);
        mtx[n].label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (mtx[n].label);
    }
}

void PatchPage::resized()
{
    cards.clear();

    auto r = getLocalBounds().reduced (6);
    constexpr int gap = 8;

    auto top = r.removeFromTop (juce::jmin (270, r.getHeight() * 11 / 20));
    r.removeFromTop (gap);
    const auto cols = splitColumns (top, { 1.25f, 1.0f, 1.1f, 1.25f }, gap);

    {
        cards.push_back ({ cols[0], "Level & Pitch" });
        auto c = contentOf (cards.back());
        placeKnobs (c.removeFromTop (ParamKnob::cellHeight * 2), { level, pan, coarse, fine, octave, analog }, 3);
        c.removeFromTop (6);
        stretch->setBounds (c.removeFromTop (40).removeFromLeft (170));
    }

    {
        cards.push_back ({ cols[1], "Playing" });
        auto c = contentOf (cards.back());
        auto row = c.removeFromTop (40);
        const int half = (row.getWidth() - 8) / 2;
        monoPoly->setBounds (row.removeFromLeft (half));
        row.removeFromLeft (8);
        priority->setBounds (row);
        c.removeFromTop (8);
        row = c.removeFromTop (40);
        legato->setBounds (row.removeFromLeft (half));
        row.removeFromLeft (8);
        retrig->setBounds (row);
        c.removeFromTop (8);
        placeKnobs (c.removeFromTop (ParamKnob::cellHeight), { bendUp, bendDown }, 2);
    }

    {
        cards.push_back ({ cols[2], "Portamento" });
        auto c = contentOf (cards.back());
        portaSw->setBounds (c.removeFromTop (40).removeFromLeft (110));
        c.removeFromTop (8);
        auto row = c.removeFromTop (40);
        const int half = (row.getWidth() - 8) / 2;
        portaMode->setBounds (row.removeFromLeft (half));
        row.removeFromLeft (8);
        portaType->setBounds (row);
        c.removeFromTop (8);
        row = c.removeFromTop (40);
        portaStart->setBounds (row.removeFromLeft (half));
        c.removeFromTop (6);
        placeKnobs (c.removeFromTop (ParamKnob::cellHeight), { portaTime }, 1);
    }

    {
        cards.push_back ({ cols[3], "Offsets, Unison & TMT" });
        auto c = contentOf (cards.back());
        placeKnobs (c.removeFromTop (ParamKnob::cellHeight * 2), { cutOfs, resoOfs, attOfs, relOfs, velOfs, unisonFat }, 3);
        c.removeFromTop (6);
        auto row = c.removeFromTop (40);
        unison->setBounds (row.removeFromLeft (100));
        row.removeFromLeft (8);
        tmtCtrl->setBounds (row.removeFromLeft (130));
    }

    {
        cards.push_back ({ r, "Matrix Control" });
        auto c = contentOf (cards.back());
        constexpr int labelW = 66, srcW = 150, destW = 118, sensW = 50, rowH = 40;
        const int blockW = destW + sensW + 8;

        for (int n = 0; n < 4; ++n)
        {
            auto row = c.removeFromTop (rowH).withTrimmedBottom (2);
            mtx[n].label.setBounds (row.removeFromLeft (labelW));
            mtx[n].src->setBounds (row.removeFromLeft (srcW).withHeight (28).withY (row.getY() + 12));
            row.removeFromLeft (10);

            for (int d = 0; d < 4; ++d)
            {
                auto cell = row.removeFromLeft (blockW);
                mtx[n].dst[d]->setBounds (cell.removeFromLeft (destW).withHeight (28).withY (cell.getY() + 12));
                cell.removeFromLeft (4);
                mtx[n].sens[d]->setBounds (cell.removeFromLeft (sensW).withHeight (38));
            }
        }
    }

    repaint();
}

//==============================================================================
TmtPage::TmtPage (MC909EditorProcessor& p) : PageBase (p)
{
    structure12 = add<ParamChoice> ("tmt.structure12", "Structure 1 & 2");
    booster12   = add<ParamChoice> ("tmt.booster12", "Booster 1 & 2");
    structure34 = add<ParamChoice> ("tmt.structure34", "Structure 3 & 4");
    booster34   = add<ParamChoice> ("tmt.booster34", "Booster 3 & 4");
    velControl  = add<ParamChoice> ("tmt.vel_control", "Velocity Control");

    for (int t = 0; t < 4; ++t)
    {
        const juce::String tag ("tmt.tmt" + juce::String (t + 1));
        tmt[t].sw      = add<ParamToggle> (tag + ".sw", "Tone " + juce::String (t + 1));
        tmt[t].keyLo   = add<ParamNumber> (tag + ".key_lo", "Key Low");
        tmt[t].keyHi   = add<ParamNumber> (tag + ".key_hi", "Key High");
        tmt[t].kfadeLo = add<ParamNumber> (tag + ".kfade_lo", "Fade Low");
        tmt[t].kfadeHi = add<ParamNumber> (tag + ".kfade_hi", "Fade High");
        tmt[t].velLo   = add<ParamNumber> (tag + ".vel_lo", "Vel Low");
        tmt[t].velHi   = add<ParamNumber> (tag + ".vel_hi", "Vel High");
        tmt[t].vfadeLo = add<ParamNumber> (tag + ".vfade_lo", "Fade Low");
        tmt[t].vfadeHi = add<ParamNumber> (tag + ".vfade_hi", "Fade High");
    }
}

void TmtPage::resized()
{
    cards.clear();

    auto r = getLocalBounds().reduced (6);
    constexpr int gap = 8;

    auto top = r.removeFromTop (92);
    r.removeFromTop (gap);

    {
        cards.push_back ({ top, "Structure" });
        auto c = contentOf (cards.back());
        const int w = (c.getWidth() - 4 * 10) / 5;
        for (auto* ch : { structure12, booster12, structure34, booster34, velControl })
        {
            ch->setBounds (c.removeFromLeft (w).withHeight (40));
            c.removeFromLeft (10);
        }
    }

    const auto cols = splitColumns (r, { 1, 1, 1, 1 }, gap);
    for (int t = 0; t < 4; ++t)
    {
        cards.push_back ({ cols[t], "TMT " + juce::String (t + 1) });
        auto c = contentOf (cards.back());

        tmt[t].sw->setBounds (c.removeFromTop (36).removeFromLeft (110));
        c.removeFromTop (10);

        const int colW = (c.getWidth() - 12) / 2;
        auto left = c.removeFromLeft (colW);
        c.removeFromLeft (12);
        auto right = c;

        ParamNumber* keys[] { tmt[t].keyLo, tmt[t].keyHi, tmt[t].kfadeLo, tmt[t].kfadeHi };
        ParamNumber* vels[] { tmt[t].velLo, tmt[t].velHi, tmt[t].vfadeLo, tmt[t].vfadeHi };

        for (int i = 0; i < 4; ++i)
        {
            keys[i]->setBounds (left.removeFromTop (40));
            left.removeFromTop (6);
            vels[i]->setBounds (right.removeFromTop (40));
            right.removeFromTop (6);
        }
    }

    repaint();
}


//==============================================================================
FxPage::FxPage (MC909EditorProcessor& p) : PageBase (p)
{
    sw[0] = add<ParamToggle> ("fx.comp_sw", "Compressor");
    sw[1] = add<ParamToggle> ("fx.mfx1_sw", "MFX 1");
    sw[2] = add<ParamToggle> ("fx.mfx2_sw", "MFX 2");
    sw[3] = add<ParamToggle> ("fx.rev_sw",  "Reverb");

    auto build = [this] (Unit& u, const juce::String& tag, int count, bool hasSend, bool hasOut)
    {
        u.type = add<ParamChoice> (tag + ".type", "Type");
        if (hasSend) u.send = add<ParamNumber> (tag + ".rev_send", "Rev Send");
        if (hasOut)  u.out  = add<ParamChoice> (tag + ".out", "Output");

        for (int n = 1; n <= count; ++n)
            u.params.push_back (add<ParamNumber> (tag + ".p" + juce::String (n), "P" + juce::String (n)));
    };

    build (mfx1, "mfx1", 32, true, true);
    build (mfx2, "mfx2", 32, true, false);
    build (rev,  "rev",  20, false, false);
}

void FxPage::layoutUnit (Unit& u, juce::Rectangle<int> c, int columns)
{
    auto row = c.removeFromTop (40);
    u.type->setBounds (row.removeFromLeft (150));
    row.removeFromLeft (8);
    if (u.send) { u.send->setBounds (row.removeFromLeft (70).withHeight (38)); row.removeFromLeft (8); }
    if (u.out)  u.out->setBounds (row.removeFromLeft (90));

    c.removeFromTop (10);
    const int cellW = (c.getWidth() - (columns - 1) * 6) / columns;
    int i = 0;
    for (auto* pn : u.params)
    {
        const int col = i % columns, rowIdx = i / columns;
        pn->setBounds (c.getX() + col * (cellW + 6), c.getY() + rowIdx * 44, cellW, 38);
        ++i;
    }
}

void FxPage::resized()
{
    cards.clear();

    auto r = getLocalBounds().reduced (6);
    constexpr int gap = 8;

    auto top = r.removeFromTop (92);
    r.removeFromTop (gap);

    {
        cards.push_back ({ top, "Effect Switches" });
        auto c = contentOf (cards.back());
        for (auto* t : sw)
        {
            t->setBounds (c.removeFromLeft (130).withHeight (36));
            c.removeFromLeft (10);
        }
    }

    const auto cols = splitColumns (r, { 1.0f, 1.0f, 0.8f }, gap);

    cards.push_back ({ cols[0], "MFX 1  -  parameters depend on the type" });
    layoutUnit (mfx1, contentOf (cards.back()), 6);

    cards.push_back ({ cols[1], "MFX 2" });
    layoutUnit (mfx2, contentOf (cards.back()), 6);

    cards.push_back ({ cols[2], "Reverb" });
    layoutUnit (rev, contentOf (cards.back()), 5);

    repaint();
}

//==============================================================================
PadGrid::PadGrid (MC909EditorProcessor& p) : ParamControl (p)
{
    for (int i = 0; i < 16; ++i)
    {
        auto& b = buttons[i];
        b.setClickingTogglesState (true);
        b.setRadioGroupId (4202);
        b.onClick = [this, i]
        {
            if (buttons[i].getToggleState())
                proc.setSelectedPad (i);
        };
        addAndMakeVisible (b);
    }
}

void PadGrid::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (22);                       // kit name

    const int cellW = r.getWidth() / 2, cellH = r.getHeight() / 8;
    for (int i = 0; i < 16; ++i)
    {
        // Pads read top to bottom, left column then right, so 1-8 / 9-16.
        const int col = i / 8, row = i % 8;
        buttons[i].setBounds (r.getX() + col * cellW, r.getY() + row * cellH, cellW - 3, cellH - 3);
    }
}

void PadGrid::paint (juce::Graphics& g)
{
    g.setColour (ui::col::textDim);
    g.setFont (ui::font (11.0f));
    const auto kit = proc.getKitName();
    g.drawText (kit.isEmpty() ? juce::String ("(no kit read)") : "Kit: " + kit,
                getLocalBounds().removeFromBottom (20), juce::Justification::centredLeft);
}

void PadGrid::refresh()
{
    const auto accent = ui::toneColour (proc.getSelectedTone());

    for (int i = 0; i < 16; ++i)
    {
        const auto name = proc.getPadName (i);
        buttons[i].setButtonText (juce::String (i + 1) + "  " + (name.isEmpty() ? juce::String ("-") : name));
        buttons[i].setColour (juce::TextButton::buttonOnColourId, accent);
        buttons[i].setToggleState (i == proc.getSelectedPad(), juce::dontSendNotification);
    }

    repaint();
}

//==============================================================================
RhythmPage::RhythmPage (MC909EditorProcessor& p) : PageBase (p)
{
    grid = add<PadGrid>();

    toneSw    = add<ParamToggle> ("rn.t.sw", "Tone");
    waveGroup = add<ParamChoice> ("rn.t.wave_group", "Wave Group");
    waveGain  = add<ParamChoice> ("rn.t.wave_gain", "Gain");
    waveGid   = add<ParamNumber> ("rn.t.wave_gid", "Group ID");
    waveL     = add<ParamNumber> ("rn.t.wave_l", "Wave L");
    waveR     = add<ParamNumber> ("rn.t.wave_r", "Wave R");
    fxmSwitch = add<ParamToggle> ("rn.t.fxm_sw", "FXM");
    fxmColor  = add<ParamChoice> ("rn.t.fxm_color", "FXM Color");
    fxmDepth  = add<ParamKnob>   ("rn.t.fxm_depth", "FXM Depth");

    wCoarse  = add<ParamKnob> ("rn.t.coarse", "Coarse");
    wFine    = add<ParamKnob> ("rn.t.fine", "Fine");
    wPan     = add<ParamKnob> ("rn.t.pan", "Pan");
    wLevel   = add<ParamKnob> ("rn.t.level", "Level");
    velLo    = add<ParamKnob> ("rn.t.vel_lo", "Vel Low");
    velHi    = add<ParamKnob> ("rn.t.vel_hi", "Vel High");
    fadeLo   = add<ParamKnob> ("rn.t.fade_lo", "Fade Low");
    fadeHi   = add<ParamKnob> ("rn.t.fade_hi", "Fade High");
    rndPanSw = add<ParamToggle> ("rn.t.rnd_pan_sw", "Rnd Pan");
    altPanSw = add<ParamToggle> ("rn.t.alt_pan_sw", "Alt Pan");

    assign  = add<ParamChoice> ("rn.assign", "Assign");
    mute    = add<ParamChoice> ("rn.mute", "Mute Group");
    envMode = add<ParamChoice> ("rn.env_mode", "Env Mode");
    level    = add<ParamKnob> ("rn.level", "Level");
    coarse   = add<ParamKnob> ("rn.coarse", "Coarse");
    fine     = add<ParamKnob> ("rn.fine", "Fine");
    rndPitch = add<ParamKnob> ("rn.rnd_pitch", "Random");
    pan      = add<ParamKnob> ("rn.pan", "Pan");
    rndPan   = add<ParamKnob> ("rn.rnd_pan", "Rnd Pan");
    altPan   = add<ParamKnob> ("rn.alt_pan", "Alt Pan");
    rev      = add<ParamKnob> ("rn.rev_send", "Reverb");
    revMfx   = add<ParamKnob> ("rn.rev_send_mfx", "Rev MFX");
    bend     = add<ParamKnob> ("rn.bend_range", "Bend");

    curve      = add<FilterCurve> (juce::String ("rn."));
    filterType = add<ParamChoice> ("rn.tvf_type", "Type", true);
    cutoff     = add<ParamKnob> ("rn.tvf_cutoff", "Cutoff");
    reso       = add<ParamKnob> ("rn.tvf_reso", "Resonance");
    cutVel     = add<ParamKnob> ("rn.tvf_cut_vsens", "Cut Vel");
    resoVel    = add<ParamKnob> ("rn.tvf_reso_vs", "Reso Vel");

    pitchEnv  = add<EnvelopeEditor> (EnvelopeEditor::Kind::pitch,  juce::String ("rn."));
    filterEnv = add<EnvelopeEditor> (EnvelopeEditor::Kind::filter, juce::String ("rn."));
    ampEnv    = add<EnvelopeEditor> (EnvelopeEditor::Kind::amp,    juce::String ("rn."));

    pDepth = add<ParamKnob> ("rn.penv_depth", "Depth");
    pVel   = add<ParamKnob> ("rn.penv_vsens", "Vel Sens");
    pT1    = add<ParamKnob> ("rn.penv_t1_vs", "T1 Vel");
    pT4    = add<ParamKnob> ("rn.penv_t4_vs", "T4 Vel");

    fDepth = add<ParamKnob> ("rn.tvf_env_depth", "Depth");
    fVel   = add<ParamKnob> ("rn.tvf_env_vsens", "Vel Sens");
    fT1    = add<ParamKnob> ("rn.tvf_t1_vs", "T1 Vel");
    fT4    = add<ParamKnob> ("rn.tvf_t4_vs", "T4 Vel");

    aVel = add<ParamKnob> ("rn.tva_vsens", "Vel Sens");
    aT1  = add<ParamKnob> ("rn.tva_t1_vs", "T1 Vel");
    aT4  = add<ParamKnob> ("rn.tva_t4_vs", "T4 Vel");
}

void RhythmPage::resized()
{
    cards.clear();

    auto r = getLocalBounds().reduced (6);
    constexpr int gap = 8;

    auto top = r.removeFromTop (juce::jmin (250, r.getHeight() * 11 / 20));
    r.removeFromTop (gap);

    const auto cols = splitColumns (top, { 0.17f, 0.27f, 0.24f, 0.32f }, gap);

    {   // PADS
        cards.push_back ({ cols[0], "Pads" });
        grid->setBounds (contentOf (cards.back()));
    }

    {   // WAVE (selected tone)
        cards.push_back ({ cols[1], "Wave  -  selected tone" });
        auto c = contentOf (cards.back());

        auto row = c.removeFromTop (40);
        toneSw->setBounds (row.removeFromLeft (58));
        row.removeFromLeft (6);
        const int half = (row.getWidth() - 6) / 2;
        waveGroup->setBounds (row.removeFromLeft (half));
        row.removeFromLeft (6);
        waveGain->setBounds (row);

        c.removeFromTop (6);
        auto nums = c.removeFromTop (40);
        const int third = (nums.getWidth() - 12) / 3;
        waveGid->setBounds (nums.removeFromLeft (third));
        nums.removeFromLeft (6);
        waveL->setBounds (nums.removeFromLeft (third));
        nums.removeFromLeft (6);
        waveR->setBounds (nums);

        c.removeFromTop (6);
        fxmSwitch->setBounds (c.getX(), c.getY(), 62, 40);
        fxmColor->setBounds (c.getX() + 70, c.getY(), 62, 40);
        fxmDepth->setBounds (c.getX() + 140, c.getY() - 4, ParamKnob::cellWidth, ParamKnob::cellHeight);
    }

    {   // TONE MIX (selected tone)
        cards.push_back ({ cols[2], "Mix & Velocity  -  selected tone" });
        auto c = contentOf (cards.back());

        placeKnobs (c.removeFromTop (ParamKnob::cellHeight), { wCoarse, wFine, wPan, wLevel }, 4);
        placeKnobs (c.removeFromTop (ParamKnob::cellHeight), { velLo, velHi, fadeLo, fadeHi }, 4);

        c.removeFromTop (4);
        rndPanSw->setBounds (c.getX(), c.getY(), 70, 30);
        altPanSw->setBounds (c.getX() + 78, c.getY(), 70, 30);
    }

    {   // PAD (all tones)
        cards.push_back ({ cols[3], "Pad  -  All Tones" });
        auto c = contentOf (cards.back());

        auto row = c.removeFromTop (40);
        const int third = (row.getWidth() - 12) / 3;
        assign->setBounds (row.removeFromLeft (third));
        row.removeFromLeft (6);
        mute->setBounds (row.removeFromLeft (third));
        row.removeFromLeft (6);
        envMode->setBounds (row);

        c.removeFromTop (4);
        placeKnobs (c, { level, coarse, fine, rndPitch, pan, rndPan, altPan, rev, revMfx, bend }, 5);
    }

    //-- bottom row ----------------------------------------------------------
    const auto bcols = splitColumns (r, { 0.28f, 0.24f, 0.24f, 0.24f }, gap);

    auto layoutEnv = [this] (juce::Rectangle<int> area, const char* title, EnvelopeEditor* env,
                             std::initializer_list<juce::Component*> knobs)
    {
        cards.push_back ({ area, title });
        auto c = contentOf (cards.back());

        auto envArea = c.removeFromTop (juce::jmax (60, c.getHeight() - ParamKnob::cellHeight - 6));
        env->setBounds (envArea);
        c.removeFromTop (6);
        placeKnobs (c, knobs, 4);
    };

    {   // FILTER
        cards.push_back ({ bcols[0], "Filter" });
        auto c = contentOf (cards.back());

        auto scope = c.removeFromTop (juce::jmax (60, c.getHeight() - ParamKnob::cellHeight - 6));
        curve->setBounds (scope);
        filterType->setBounds (scope.getX() + 6, scope.getY() + 6, 96, 24);

        c.removeFromTop (6);
        placeKnobs (c, { cutoff, reso, cutVel, resoVel }, 4);
    }

    layoutEnv (bcols[1], "Pitch Envelope",  pitchEnv,  { pDepth, pVel, pT1, pT4 });
    layoutEnv (bcols[2], "Filter Envelope", filterEnv, { fDepth, fVel, fT1, fT4 });
    layoutEnv (bcols[3], "Amp Envelope",    ampEnv,    { aVel, aT1, aT4 });

    repaint();
}
