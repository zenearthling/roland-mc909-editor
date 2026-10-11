#include "Pages.h"
#include "FxData.h"

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

    const char* probeNames[3] { "Probe MFX 1", "Probe MFX 2", "Probe Reverb" };
    for (int i = 0; i < 3; ++i)
    {
        probeBtn[i].setButtonText (probeNames[i]);
        probeBtn[i].setTooltip ("Test tool: writes 1, 2, 3 ... into the effect's raw slots so a photo of the unit's screen shows "
                                "which slot feeds which row. Changes the unit's edit buffer only; re-select the patch to undo.");
        probeBtn[i].onClick = [this, i] { probe (i); };
        addAndMakeVisible (probeBtn[i]);
    }

    build (mfx1, "mfx1", 32, true, true);
    build (mfx2, "mfx2", 32, true, false);
    build (rev,  "rev",  20, false, false);
}

void FxPage::probe (int unit)
{
    const char* tags[3] { "mfx1", "mfx2", "rev" };
    const int count = unit == 2 ? 20 : 32;
    for (int n = 1; n <= count; ++n)
    {
        const auto& def = ui::findParam (juce::String (tags[unit]) + ".p" + juce::String (n));
        proc.setParameterValue (def, def.dispSub + n);
    }
}

void FxPage::refresh()
{
    PageBase::refresh();

    Unit* units[3] { &mfx1, &mfx2, &rev };
    const char* tags[3] { "mfx1.type", "mfx2.type", "rev.type" };

    for (int i = 0; i < 3; ++i)
    {
        const int type = proc.getParameterValue (ui::findParam (tags[i]));
        if (type != lastType[i])
            relabel (i, type);
    }
}

void FxPage::relabel (int unit, int type)
{
    lastType[unit] = type;
    Unit& u = unit == 0 ? mfx1 : unit == 1 ? mfx2 : rev;

    const auto& names  = unit == 2 ? fxdata::reverbTypeNames() : fxdata::mfxTypeNames();
    const auto& params = unit == 2 ? fxdata::reverbParams (type) : fxdata::mfxParams (type);
    const juce::String typeName = juce::isPositiveAndBelow (type, names.size()) ? names[type] : juce::String ("?");

    const char* heads[3] { "MFX 1", "MFX 2", "Reverb" };
    unitName[unit] = juce::String (heads[unit]) + "  -  " + typeName;

    for (size_t i = 0; i < u.params.size(); ++i)
    {
        ui::NumberSpec s;
        if (i < params.size() && params[i].label != "-")
        {
            const auto& pr = params[i];
            s.active = true;
            s.label = pr.label;
            s.typeName = typeName;
            s.kind = pr.kind == fxdata::Param::Kind::integer ? ui::NumberSpec::Kind::integer
                   : pr.kind == fxdata::Param::Kind::choice  ? ui::NumberSpec::Kind::choice
                                                             : ui::NumberSpec::Kind::free;
            s.lo = pr.lo; s.hi = pr.hi; s.options = pr.options; s.unit = pr.unit; s.hint = pr.hint;
        }
        u.params[i]->setSpec (s);
        u.params[i]->setVisible (s.active);
    }

    repaint();
    resized();
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
        if (! pn->isVisible())
            continue;
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
        c.removeFromLeft (30);
        for (auto& b : probeBtn)
        {
            b.setBounds (c.removeFromLeft (110).withHeight (30));
            c.removeFromLeft (6);
        }
    }

    const auto cols = splitColumns (r, { 1.0f, 1.0f, 0.8f }, gap);

    cards.push_back ({ cols[0], unitName[0] });
    layoutUnit (mfx1, contentOf (cards.back()), 3);

    cards.push_back ({ cols[1], unitName[1] });
    layoutUnit (mfx2, contentOf (cards.back()), 3);

    cards.push_back ({ cols[2], unitName[2] });
    layoutUnit (rev, contentOf (cards.back()), 3);

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


//==============================================================================
RoutingPage::RoutingPage (MC909EditorProcessor& p) : PageBase (p)
{
    partOut  = add<ParamChoice> ("part.output", "Output");
    partSend = add<ParamNumber> ("part.rev_send", "Reverb Send");
    compOut  = add<ParamChoice> ("comp.output", "Output");
    compSend = add<ParamNumber> ("comp.rev_send", "Reverb Send");
    mfx1Type = add<ParamChoice> ("mfx1.type", "Type");
    mfx1Out  = add<ParamChoice> ("mfx1.out", "Output");
    mfx1Send = add<ParamNumber> ("mfx1.rev_send", "Reverb Send");
    mfx2Type = add<ParamChoice> ("mfx2.type", "Type");
    mfx2Send = add<ParamNumber> ("mfx2.rev_send", "Reverb Send");
    revType  = add<ParamChoice> ("rev.type", "Type");
}

void RoutingPage::resized()
{
    auto r = getLocalBounds().reduced (14, 12);
    const int outW = 110, gapX = 56;
    const int colW = (r.getWidth() - outW - gapX * 3) / 3;
    const int x0 = r.getX(), x1 = x0 + colW + gapX, x2 = x1 + colW + gapX, x3 = x2 + colW + gapX;
    const int y = r.getY();

    partBox = { x0, y, colW, 118 };
    compBox = { x0, y + 150, colW, 118 };
    mfx1Box = { x1, y, colW, 162 };
    mfx2Box = { x1, y + 190, colW, 118 };
    revBox  = { x2, y + 190, colW, 70 };
    mixBox  = { x3, y, outW, 44 };
    dir1Box = { x3, y + 100, outW, 44 };
    dir2Box = { x3, y + 190, outW, 44 };

    auto stack = [] (juce::Rectangle<int> box, std::initializer_list<juce::Component*> items, int h)
    {
        auto c = box.reduced (10, 6).withTrimmedTop (20);
        for (auto* i : items) { i->setBounds (c.removeFromTop (h)); }
    };

    stack (partBox, { partOut, partSend }, 44);
    stack (compBox, { compOut, compSend }, 44);
    stack (mfx1Box, { mfx1Type, mfx1Out, mfx1Send }, 44);
    stack (mfx2Box, { mfx2Type, mfx2Send }, 44);
    stack (revBox,  { revType }, 44);
}

void RoutingPage::refresh()
{
    PageBase::refresh();
    repaint();
}

void RoutingPage::paint (juce::Graphics& g)
{
    auto val = [this] (const char* id) { return (int) proc.getParameterValue (ui::findParam (id)); };

    const int po = val ("part.output"), co = val ("comp.output"), m1o = val ("mfx1.out");
    const auto accent = juce::Colour (0xff5cc8ff), dim = juce::Colour (0xff6b7280), rv = juce::Colour (0xffd9a441);

    auto right = [] (juce::Rectangle<int> b) { return juce::Point<float> ((float) b.getRight(), (float) b.getCentreY()); };
    auto left  = [] (juce::Rectangle<int> b) { return juce::Point<float> ((float) b.getX(), (float) b.getCentreY()); };
    auto below = [] (juce::Rectangle<int> b) { return juce::Point<float> ((float) b.getCentreX(), (float) b.getBottom()); };
    auto above = [] (juce::Rectangle<int> b) { return juce::Point<float> ((float) b.getCentreX(), (float) b.getY()); };

    auto wire = [&g] (juce::Point<float> a, juce::Point<float> b, juce::Colour c, bool dashed, bool vertical, float lane)
    {
        juce::Path p;
        p.startNewSubPath (a);
        if (vertical) p.cubicTo (a.x, a.y + 40.0f, b.x, b.y - 40.0f, b.x, b.y);
        else          p.cubicTo (a.x + 40.0f + lane, a.y, b.x - 40.0f - lane, b.y, b.x, b.y);

        g.setColour (c);
        juce::PathStrokeType st (dashed ? 1.4f : 2.4f);
        if (dashed)
        {
            juce::Path d;
            const float dl[] { 5.0f, 4.0f };
            st.createDashedStroke (d, p, dl, 2);
            g.fillPath (d);
        }
        else g.strokePath (p, st);
    };

    // audio paths
    auto dest = [&] (int what, juce::Rectangle<int> src, bool sameColumnDown, int idx, float lane)
    {
        // what: 0 mix, 1 mfx1, 2 mfx2, 3 comp, 4 dir1, 5 dir2
        const auto from = right (src);
        switch (what)
        {
            case 0: wire (from, left (mixBox),  accent, false, false, lane); break;
            case 1: wire (from, left (mfx1Box), accent, false, false, lane); break;
            case 2: wire (from, left (mfx2Box), accent, false, false, lane); break;
            case 3: wire (below (src), above (compBox), accent, false, true, 0.0f); break;
            case 4: wire (from, left (dir1Box), accent, false, false, lane); break;
            case 5: wire (from, left (dir2Box), accent, false, false, lane); break;
            default: break;
        }
        juce::ignoreUnused (sameColumnDown, idx);
    };

    dest (po == 6 ? -1 : po, partBox, false, 0, 120.0f);
    dest (co, compBox, false, 1, 150.0f);
    dest (m1o == 1 ? 2 : 0, mfx1Box, false, 2, 20.0f);
    dest (0, mfx2Box, false, 3, 20.0f);
    wire (right (revBox), left (mixBox), accent, false, false, 20.0f);

    // reverb sends
    auto send = [&] (const char* id, juce::Rectangle<int> src)
    {
        if (val (id) > 0)
            wire (below (src), above (revBox), rv, true, true, 0.0f);
    };
    send ("part.rev_send", partBox);
    send ("comp.rev_send", compBox);
    send ("mfx1.rev_send", mfx1Box);
    send ("mfx2.rev_send", mfx2Box);

    auto card = [&g] (juce::Rectangle<int> b, const juce::String& title)
    {
        g.setColour (ui::col::card);
        g.fillRoundedRectangle (b.toFloat(), 8.0f);
        g.setColour (ui::col::cardEdge);
        g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (ui::col::textDim);
        g.setFont (ui::font (11.0f, true).withExtraKerningFactor (0.14f));
        g.drawText (title.toUpperCase(), b.getX() + 12, b.getY() + 4, b.getWidth() - 20, 18, juce::Justification::centredLeft);
    };

    card (partBox, "Part " + juce::String (proc.getSelectedPart() + 1));
    card (compBox, "Comp / EQ");
    card (mfx1Box, "MFX 1");
    card (mfx2Box, "MFX 2  (to mix)");
    card (revBox,  "Reverb  (to mix)");

    auto out = [&] (juce::Rectangle<int> b, const juce::String& t)
    {
        g.setColour (ui::col::well);
        g.fillRoundedRectangle (b.toFloat(), 6.0f);
        g.setColour (accent);
        g.drawRoundedRectangle (b.toFloat().reduced (0.5f), 6.0f, 1.2f);
        g.setColour (ui::col::text);
        g.setFont (ui::font (13.0f, true));
        g.drawText (t, b, juce::Justification::centred);
    };
    out (mixBox, "MIX");
    out (dir1Box, "DIR 1");
    out (dir2Box, "DIR 2");

    g.setColour (ui::col::textDim);
    g.setFont (ui::font (11.0f));
    auto legend = getLocalBounds().reduced (14, 12).removeFromBottom (40);
    g.drawText ("Solid blue = audio path.  Dashed gold = reverb send (shown when the send is above 0).  "
                "Part output RHYTHM follows the rhythm settings, so no wire is drawn.",
                legend, juce::Justification::bottomLeft);
}
