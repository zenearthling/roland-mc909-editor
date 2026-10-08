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
