#pragma once
#include "UiKit.h"

/** Name of the selected wave and a button that opens the waveform browser. Works for a
    patch tone ("tone.") and a rhythm pad tone ("rn.t."). */
class WaveBar : public ui::ParamControl
{
public:
    WaveBar (MC909EditorProcessor&, const juce::String& rootPrefix);
    void resized() override;
    void refresh() override;

    std::function<void()> onBrowse;

private:
    juce::String root;
    juce::Label nameLabel;
    juce::TextButton browse { "Browse..." };
};

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
    WaveBar         *waveBar;

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

/** Patch-level settings: level/pitch, playing behaviour, portamento, offsets, unison and the
    four matrix controls. */
class PatchPage : public ui::PageBase
{
public:
    explicit PatchPage (MC909EditorProcessor&);
    void resized() override;

private:
    ui::ParamKnob *level, *pan, *coarse, *fine, *octave, *analog;
    ui::ParamChoice *stretch, *monoPoly, *priority, *portaMode, *portaType, *portaStart;
    ui::ParamToggle *legato, *retrig, *portaSw, *tmtCtrl, *unison;
    ui::ParamKnob *bendUp, *bendDown, *portaTime;
    ui::ParamKnob *cutOfs, *resoOfs, *attOfs, *relOfs, *velOfs, *unisonFat;

    struct Matrix
    {
        ui::ParamChoice* src;
        ui::ParamChoice* dst[4];
        ui::ParamNumber* sens[4];
        juce::Label label;
    };
    Matrix mtx[4];
};

/** Tone Mix Table: how the four tones are combined, and each tone's key and velocity window. */
class TmtPage : public ui::PageBase
{
public:
    explicit TmtPage (MC909EditorProcessor&);
    void resized() override;

private:
    ui::ParamChoice *structure12, *structure34, *booster12, *booster34, *velControl;

    struct Tmt
    {
        ui::ParamToggle* sw;
        ui::ParamNumber *keyLo, *keyHi, *kfadeLo, *kfadeHi, *velLo, *velHi, *vfadeLo, *vfadeHi;
    };
    Tmt tmt[4] {};
};

/** Effect switches plus MFX 1, MFX 2 and Reverb. The MIDI document gives each MFX a type and
    32 raw parameters but not what they mean per type, so those are shown as P1... numbers. */
class FxPage : public ui::PageBase
{
public:
    explicit FxPage (MC909EditorProcessor&);
    void resized() override;
    void refresh() override;

private:
    void relabel (int unit, int type);
    int lastType[3] { -1, -1, -1 };
    juce::String unitName[3] { "MFX 1", "MFX 2", "Reverb" };

    ui::ParamToggle *sw[4];
    juce::TextButton probeBtn[3];
    void probe (int unit);

    struct Unit
    {
        ui::ParamChoice *type = nullptr, *out = nullptr;
        ui::ParamNumber* send = nullptr;
        std::vector<ui::ParamNumber*> params;
    };
    Unit mfx1, mfx2, rev;

    void layoutUnit (Unit&, juce::Rectangle<int> content, int columns);
};

/** The unit's Routing screen as a flow chart: Part and Comp feed MFX 1 / MFX 2 / the outputs, and each
    box has a reverb send. Wires are redrawn from the live values; every box edits its real parameters. */
class RoutingPage : public ui::PageBase
{
public:
    explicit RoutingPage (MC909EditorProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    ui::ParamChoice *partOut, *compOut, *mfx1Type, *mfx1Out, *mfx2Type, *revType;
    ui::ParamNumber *partSend, *compSend, *mfx1Send, *mfx2Send;
    juce::Rectangle<int> partBox, compBox, mfx1Box, mfx2Box, revBox, mixBox, dir1Box, dir2Box;
};

/** One pad button: number, then the wave name at a fixed size, and a flash while its key plays. */
class PadButton : public juce::TextButton
{
public:
    void setPad (int number, const juce::String& waveName);
    void setLit (bool on);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    int padNumber = 1;
    juce::String name;
    bool lit = false;
};

/** The 16 pads of a rhythm kit, plus the kit name underneath. The pads light up when their
    key (59 + pad) arrives on the part's MIDI channel, from Live, the MPK or the unit. */
class PadGrid : public ui::ParamControl,
                private juce::Timer
{
public:
    explicit PadGrid (MC909EditorProcessor&);
    ~PadGrid() override { stopTimer(); }
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh() override;

private:
    void timerCallback() override;

    PadButton buttons[16];
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
    WaveBar         *waveBar;
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
