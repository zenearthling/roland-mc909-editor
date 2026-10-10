#pragma once
#include "UiKit.h"

/** Main sound page: wave, pitch, filter and amp cards on top, the three
    envelopes underneath. */
class TonePage : public ui::PageBase
{
public:
    explicit TonePage (MC909EditorProcessor&);
    void resized() override;

private:
    ui::ParamChoice *waveGroup, *waveGain, *fxmColor;
    ui::ParamNumber *waveGid, *waveL, *waveR;
    ui::ParamToggle *fxmSwitch;
    ui::ParamKnob   *fxmDepth;

    ui::ParamKnob *coarse, *fine, *rndPitch, *pitchKf;

    ui::FilterCurve *curve;
    ui::ParamChoice *filterType;
    ui::ParamKnob   *cutoff, *reso, *cutKf, *cutVel, *resoVel;

    ui::ParamKnob *level, *pan, *panKf, *rndPan, *altPan;

    ui::EnvelopeEditor *pitchEnv, *filterEnv, *ampEnv;
    ui::ParamKnob *pDepth, *pVel, *pT1, *pT4, *pKf;
    ui::ParamKnob *fDepth, *fVel, *fT1, *fT4, *fKf;
    ui::ParamKnob *aVel, *aT1, *aT4, *aKf;
};

/** Both LFOs, each with a schematic scope and its full set of controls. */
class ModPage : public ui::PageBase
{
public:
    explicit ModPage (MC909EditorProcessor&);
    void resized() override;

private:
    struct Lfo
    {
        ui::LfoScope* scope;
        ui::ParamChoice *offset, *fadeMode;
        ui::ParamToggle* keyTrig;
        ui::ParamKnob *rate, *detune, *wave, *delay, *delayKf, *fadeTime, *pitchDep, *tvfDep, *tvaDep, *panDep;
    };

    Lfo lfo[2] {};
};

/** Sends, playback behaviour, receive switches, bias and tone-control switches. */
class OutPage : public ui::PageBase
{
public:
    explicit OutPage (MC909EditorProcessor&);
    void resized() override;

private:
    ui::ParamKnob *dry, *revMfx, *rev;

    ui::ParamChoice *envMode, *delayMode, *panMode, *biasDir;
    ui::ParamKnob   *delayTime, *biasLevel, *biasPos;
    ui::ParamToggle *tempoSync, *redamper, *rxBender, *rxExpr, *rxHold;

    ui::ParamChoice* ctrl[4][4] {};
    juce::Label rowLabels[4], colLabels[4];
};

/** The 16 pads of a rhythm kit, plus the kit name underneath. */
class PadGrid : public ui::ParamControl
{
public:
    explicit PadGrid (MC909EditorProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    juce::TextButton buttons[16];
};

/** Rhythm kit editor: pick a pad, then edit its tone sections (wave, mix, velocity range),
    the pad-wide pitch/pan/send settings, filter and the three envelopes. Tone 1-4 in the
    header chooses which tone section the Wave and Tone cards show. */
class RhythmPage : public ui::PageBase
{
public:
    explicit RhythmPage (MC909EditorProcessor&);
    void resized() override;

private:
    PadGrid* grid;

    // per-tone
    ui::ParamToggle *toneSw, *fxmSwitch, *rndPanSw, *altPanSw;
    ui::ParamChoice *waveGroup, *waveGain, *fxmColor;
    ui::ParamNumber *waveGid, *waveL, *waveR;
    ui::ParamKnob   *fxmDepth, *wCoarse, *wFine, *wPan, *wLevel, *velLo, *velHi, *fadeLo, *fadeHi;

    // pad-wide
    ui::ParamChoice *assign, *mute, *envMode;
    ui::ParamKnob   *level, *coarse, *fine, *rndPitch, *pan, *rndPan, *altPan, *rev, *revMfx, *bend;

    ui::FilterCurve *curve;
    ui::ParamChoice *filterType;
    ui::ParamKnob   *cutoff, *reso, *cutVel, *resoVel;

    ui::EnvelopeEditor *pitchEnv, *filterEnv, *ampEnv;
    ui::ParamKnob *pDepth, *pVel, *pT1, *pT4;
    ui::ParamKnob *fDepth, *fVel, *fT1, *fT4;
    ui::ParamKnob *aVel, *aT1, *aT4;
};
