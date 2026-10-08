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
