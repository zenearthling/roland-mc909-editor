#include "MC909Map.h"

namespace mc909
{

namespace
{
    using SA = juce::StringArray;

    const SA offOn        { "OFF", "ON" };
    const SA offOnRev     { "OFF", "ON", "REVERSE" };
    const SA offOnRandom  { "OFF", "ON", "RANDOM" };
    const SA envMode      { "NO-SUS", "SUSTAIN" };
    const SA delayMode    { "NORMAL", "HOLD", "KEY-OFF-NORMAL", "KEY-OFF-DECAY" };
    const SA panMode      { "CONTINUOUS", "KEY-ON" };
    const SA waveGroup    { "INT", "---", "SRX", "SAMPLE" };
    const SA waveGain     { "-6 dB", "0 dB", "+6 dB", "+12 dB" };
    const SA fxmColor     { "1", "2", "3", "4" };
    const SA filterType   { "OFF", "LPF", "BPF", "HPF", "PKG", "LPF2", "LPF3" };
    const SA velCurve     { "FIXED", "1", "2", "3", "4", "5", "6", "7" };
    const SA biasDir      { "LOWER", "UPPER", "LOWER&UPPER", "ALL" };
    const SA lfoOffset    { "-100", "-50", "0", "+50", "+100" };
    const SA fadeMode     { "ON-IN", "ON-OUT", "OFF-IN", "OFF-OUT" };
    const SA monoPoly     { "MONO", "POLY" };
    const SA priority     { "LAST", "LOUDEST" };
    const SA portaMode    { "NORMAL", "LEGATO" };
    const SA portaType    { "RATE", "TIME" };
    const SA portaStart   { "PITCH", "NOTE" };
    const SA stretchTune  { "OFF", "1", "2", "3" };
    const SA outSelect    { "DRY", "MFX1", "MFX2", "COMP", "DIR1", "DIR2", "RHYTHM" };
    const SA compOutAsgn  { "DRY", "MFX1", "MFX2" };
    const SA compLowFreq  { "200 Hz", "400 Hz" };
    const SA compHighFreq { "2000 Hz", "4000 Hz", "8000 Hz" };

    const SA matrixSource = []
    {
        SA s; s.add ("OFF");
        for (int cc = 1;  cc <= 31; ++cc) s.add ("CC" + juce::String (cc));
        for (int cc = 33; cc <= 95; ++cc) s.add ("CC" + juce::String (cc));
        s.addArray (SA { "BEND", "AFT", "SYS1", "SYS2", "SYS3", "SYS4", "VELOCITY",
                         "KEYFOLLOW", "TEMPO", "LFO1", "LFO2",
                         "PIT-ENV", "TVF-ENV", "TVA-ENV" });
        return s;
    }();

    const SA matrixDest { "OFF", "PCH", "CUT", "RES", "LEV", "PAN", "DRY", "---", "REV",
                          "PIT-LFO1", "PIT-LFO2", "TVF-LFO1", "TVF-LFO2",
                          "TVA-LFO1", "TVA-LFO2", "PAN-LFO1", "PAN-LFO2",
                          "LFO1-RATE", "LFO2-RATE",
                          "PIT-ATK", "PIT-DCY", "PIT-REL",
                          "TVF-ATK", "TVF-DCY", "TVF-REL",
                          "TVA-ATK", "TVA-DCY", "TVA-REL", "TMT", "FXM" };

    std::vector<ParamDef> gTable;

    void add (Block b, const char* id, const char* name, uint32_t offset,
              int rawMin, int rawMax,
              int dispSub = 0, int dispMul = 1,
              const SA& choices = {}, int numBytes = 1)
    {
        ParamDef p;
        p.id       = id;
        p.name     = name;
        p.block    = b;
        p.offset   = offset;
        p.numBytes = numBytes;
        p.rawMin   = rawMin;
        p.rawMax   = rawMax;
        p.dispSub  = dispSub;
        p.dispMul  = dispMul;
        p.choices  = choices;
        gTable.push_back (std::move (p));
    }

    // -----------------------------------------------------------------------
    void buildPatchCommon()
    {
        const auto B = Block::patchCommon;

        add (B, "patch.category",      "Category",          0x0C, 0, 127);
        add (B, "patch.level",         "Patch Level",       0x0E, 0, 127);
        add (B, "patch.pan",           "Patch Pan",         0x0F, 0, 127, 64);
        add (B, "patch.priority",      "Priority",          0x10, 0, 1, 0, 1, priority);
        add (B, "patch.coarse",        "Coarse Tune",       0x11, 16, 112, 64);
        add (B, "patch.fine",          "Fine Tune",         0x12, 14, 114, 64);
        add (B, "patch.octave",        "Octave Shift",      0x13, 61, 67, 64);
        add (B, "patch.stretch",       "Stretch Tune",      0x14, 0, 3, 0, 1, stretchTune);
        add (B, "patch.analog_feel",   "Analog Feel",       0x15, 0, 127);
        add (B, "patch.mono_poly",     "Mono/Poly",         0x16, 0, 1, 0, 1, monoPoly);
        add (B, "patch.legato",        "Legato Switch",     0x17, 0, 1, 0, 1, offOn);
        add (B, "patch.legato_retrig", "Legato Retrigger",  0x18, 0, 1, 0, 1, offOn);
        add (B, "patch.porta_sw",      "Portamento",        0x19, 0, 1, 0, 1, offOn);
        add (B, "patch.porta_mode",    "Porta Mode",        0x1A, 0, 1, 0, 1, portaMode);
        add (B, "patch.porta_type",    "Porta Type",        0x1B, 0, 1, 0, 1, portaType);
        add (B, "patch.porta_start",   "Porta Start",       0x1C, 0, 1, 0, 1, portaStart);
        add (B, "patch.porta_time",    "Porta Time",        0x1D, 0, 127);

        add (B, "patch.cutoff_ofs",    "Cutoff Offset",     0x22, 1, 127, 64);
        add (B, "patch.reso_ofs",      "Resonance Offset",  0x23, 1, 127, 64);
        add (B, "patch.attack_ofs",    "Attack Offset",     0x24, 1, 127, 64);
        add (B, "patch.release_ofs",   "Release Offset",    0x25, 1, 127, 64);
        add (B, "patch.velsens_ofs",   "Velocity Sens Offset", 0x26, 1, 127, 64);

        add (B, "patch.tmt_ctrl",      "TMT Control",       0x28, 0, 1, 0, 1, offOn);
        add (B, "patch.bend_up",       "Bend Range Up",     0x29, 0, 48);
        add (B, "patch.bend_down",     "Bend Range Down",   0x2A, 0, 48);

        // Matrix Control 1-4: source at 0x2B + n*9, then 4 x (dest, sens).
        for (int n = 0; n < 4; ++n)
        {
            const uint32_t base = 0x2B + (uint32_t) n * 9;
            const juce::String tag ("mtx" + juce::String (n + 1));

            add (B, ("patch." + tag + ".src").toRawUTF8(),
                    ("Matrix " + juce::String (n + 1) + " Source").toRawUTF8(),
                    base, 0, 109, 0, 1, matrixSource);

            for (int d = 0; d < 4; ++d)
            {
                add (B, ("patch." + tag + ".dst" + juce::String (d + 1)).toRawUTF8(),
                        ("Matrix " + juce::String (n + 1) + " Dest " + juce::String (d + 1)).toRawUTF8(),
                        base + 1 + (uint32_t) d * 2, 0, 29, 0, 1, matrixDest);

                add (B, ("patch." + tag + ".sens" + juce::String (d + 1)).toRawUTF8(),
                        ("Matrix " + juce::String (n + 1) + " Sens " + juce::String (d + 1)).toRawUTF8(),
                        base + 2 + (uint32_t) d * 2, 1, 127, 64);
            }
        }

        add (B, "patch.unison_sw",  "Unison",          0x4F, 0, 1, 0, 1, offOn);
        add (B, "patch.unison_fat", "Unison Fat Level", 0x50, 0, 127);
    }

    // -----------------------------------------------------------------------
    void buildPatchTMT()
    {
        const auto B = Block::patchTMT;

        add (B, "tmt.structure12", "Structure 1&2", 0x00, 0, 9, -1);
        add (B, "tmt.booster12",   "Booster 1&2",   0x01, 0, 3, 0, 1,
             SA { "0 dB", "+6 dB", "+12 dB", "+18 dB" });
        add (B, "tmt.structure34", "Structure 3&4", 0x02, 0, 9, -1);
        add (B, "tmt.booster34",   "Booster 3&4",   0x03, 0, 3, 0, 1,
             SA { "0 dB", "+6 dB", "+12 dB", "+18 dB" });
        add (B, "tmt.vel_control", "Velocity Control", 0x04, 0, 2, 0, 1, offOnRandom);

        for (int t = 0; t < 4; ++t)
        {
            const uint32_t base = 0x05 + (uint32_t) t * 9;
            const juce::String tag ("tmt" + juce::String (t + 1));
            const juce::String pre ("TMT " + juce::String (t + 1) + " ");

            add (B, ("tmt." + tag + ".sw").toRawUTF8(),        (pre + "Switch").toRawUTF8(),        base + 0, 0, 1, 0, 1, offOn);
            add (B, ("tmt." + tag + ".key_lo").toRawUTF8(),    (pre + "Key Lower").toRawUTF8(),     base + 1, 0, 127);
            add (B, ("tmt." + tag + ".key_hi").toRawUTF8(),    (pre + "Key Upper").toRawUTF8(),     base + 2, 0, 127);
            add (B, ("tmt." + tag + ".kfade_lo").toRawUTF8(),  (pre + "Key Fade Lower").toRawUTF8(), base + 3, 0, 127);
            add (B, ("tmt." + tag + ".kfade_hi").toRawUTF8(),  (pre + "Key Fade Upper").toRawUTF8(), base + 4, 0, 127);
            add (B, ("tmt." + tag + ".vel_lo").toRawUTF8(),    (pre + "Vel Lower").toRawUTF8(),     base + 5, 1, 127);
            add (B, ("tmt." + tag + ".vel_hi").toRawUTF8(),    (pre + "Vel Upper").toRawUTF8(),     base + 6, 1, 127);
            add (B, ("tmt." + tag + ".vfade_lo").toRawUTF8(),  (pre + "Vel Fade Lower").toRawUTF8(), base + 7, 0, 127);
            add (B, ("tmt." + tag + ".vfade_hi").toRawUTF8(),  (pre + "Vel Fade Upper").toRawUTF8(), base + 8, 0, 127);
        }
    }

    // -----------------------------------------------------------------------
    void buildPatchTone()
    {
        const auto B = Block::patchTone;
        constexpr uint32_t P1 = 128;   // offsets printed as "01 xx"

        add (B, "tone.level",        "Tone Level",       0x00, 0, 127);
        add (B, "tone.coarse",       "Coarse Tune",      0x01, 16, 112, 64);
        add (B, "tone.fine",         "Fine Tune",        0x02, 14, 114, 64);
        add (B, "tone.rnd_pitch",    "Random Pitch",     0x03, 0, 30);
        add (B, "tone.pan",          "Pan",              0x04, 0, 127, 64);
        add (B, "tone.pan_kf",       "Pan Keyfollow",    0x05, 54, 74, 64, 10);
        add (B, "tone.rnd_pan",      "Random Pan Depth", 0x06, 0, 63);
        add (B, "tone.alt_pan",      "Alternate Pan",    0x07, 1, 127, 64);
        add (B, "tone.env_mode",     "Env Mode",         0x08, 0, 1, 0, 1, envMode);
        add (B, "tone.delay_mode",   "Delay Mode",       0x09, 0, 3, 0, 1, delayMode);
        add (B, "tone.delay_time",   "Delay Time",       0x0A, 0, 149, 0, 1, {}, 2);
        add (B, "tone.dry_send",     "Dry Send Level",   0x0C, 0, 127);
        add (B, "tone.rev_send_mfx", "Reverb Send (MFX)", 0x0E, 0, 127);
        add (B, "tone.rev_send",     "Reverb Send",      0x10, 0, 127);
        add (B, "tone.rx_bender",    "Receive Bender",   0x12, 0, 1, 0, 1, offOn);
        add (B, "tone.rx_expr",      "Receive Expression", 0x13, 0, 1, 0, 1, offOn);
        add (B, "tone.rx_hold",      "Receive Hold-1",   0x14, 0, 1, 0, 1, offOn);
        add (B, "tone.rx_pan_mode",  "Receive Pan Mode", 0x15, 0, 1, 0, 1, panMode);
        add (B, "tone.redamper",     "Redamper",         0x16, 0, 1, 0, 1, offOn);

        // Tone Control switches 1-4 x 1-4
        for (int c = 0; c < 4; ++c)
            for (int s = 0; s < 4; ++s)
                add (B, ("tone.ctrl" + juce::String (c + 1) + "_sw" + juce::String (s + 1)).toRawUTF8(),
                        ("Tone Ctrl " + juce::String (c + 1) + " Sw " + juce::String (s + 1)).toRawUTF8(),
                        0x17 + (uint32_t) (c * 4 + s), 0, 2, 0, 1, offOnRev);

        add (B, "tone.wave_group",   "Wave Group",       0x27, 0, 3, 0, 1, waveGroup);
        add (B, "tone.wave_gid",     "Wave Group ID",    0x28, 0, 16384, 0, 1, {}, 4);
        add (B, "tone.wave_l",       "Wave Number L",    0x2C, 0, 16384, 0, 1, {}, 4);
        add (B, "tone.wave_r",       "Wave Number R",    0x30, 0, 16384, 0, 1, {}, 4);
        add (B, "tone.wave_gain",    "Wave Gain",        0x34, 0, 3, 0, 1, waveGain);
        add (B, "tone.fxm_sw",       "FXM Switch",       0x35, 0, 1, 0, 1, offOn);
        add (B, "tone.fxm_color",    "FXM Color",        0x36, 0, 3, 0, 1, fxmColor);
        add (B, "tone.fxm_depth",    "FXM Depth",        0x37, 0, 16);
        add (B, "tone.tempo_sync",   "Tempo Sync",       0x38, 0, 1, 0, 1, offOn);
        add (B, "tone.pitch_kf",     "Pitch Keyfollow",  0x39, 44, 84, 64, 10);

        add (B, "tone.penv_depth",   "Pitch Env Depth",  0x3A, 52, 76, 64);
        add (B, "tone.penv_vsens",   "Pitch Env V-Sens", 0x3B, 1, 127, 64);
        add (B, "tone.penv_t1_vs",   "P-Env T1 V-Sens",  0x3C, 1, 127, 64);
        add (B, "tone.penv_t4_vs",   "P-Env T4 V-Sens",  0x3D, 1, 127, 64);
        add (B, "tone.penv_t_kf",    "P-Env Time KF",    0x3E, 54, 74, 64, 10);
        for (int i = 0; i < 4; ++i)
            add (B, ("tone.penv_t" + juce::String (i + 1)).toRawUTF8(),
                    ("Pitch Env Time " + juce::String (i + 1)).toRawUTF8(),
                    0x3F + (uint32_t) i, 0, 127);
        for (int i = 0; i < 5; ++i)
            add (B, ("tone.penv_l" + juce::String (i)).toRawUTF8(),
                    ("Pitch Env Level " + juce::String (i)).toRawUTF8(),
                    0x43 + (uint32_t) i, 1, 127, 64);

        add (B, "tone.tvf_type",     "Filter Type",      0x48, 0, 6, 0, 1, filterType);
        add (B, "tone.tvf_cutoff",   "Cutoff",           0x49, 0, 127);
        add (B, "tone.tvf_cut_kf",   "Cutoff Keyfollow", 0x4A, 44, 84, 64, 10);
        add (B, "tone.tvf_cut_vcrv", "Cutoff Vel Curve", 0x4B, 0, 7, 0, 1, velCurve);
        add (B, "tone.tvf_cut_vsens","Cutoff Vel Sens",  0x4C, 1, 127, 64);
        add (B, "tone.tvf_reso",     "Resonance",        0x4D, 0, 127);
        add (B, "tone.tvf_reso_vs",  "Resonance V-Sens", 0x4E, 1, 127, 64);
        add (B, "tone.tvf_env_depth","TVF Env Depth",    0x4F, 1, 127, 64);
        add (B, "tone.tvf_env_vcrv", "TVF Env Vel Curve", 0x50, 0, 7, 0, 1, velCurve);
        add (B, "tone.tvf_env_vsens","TVF Env V-Sens",   0x51, 1, 127, 64);
        add (B, "tone.tvf_t1_vs",    "TVF T1 V-Sens",    0x52, 1, 127, 64);
        add (B, "tone.tvf_t4_vs",    "TVF T4 V-Sens",    0x53, 1, 127, 64);
        add (B, "tone.tvf_t_kf",     "TVF Time KF",      0x54, 54, 74, 64, 10);
        for (int i = 0; i < 4; ++i)
            add (B, ("tone.tvf_t" + juce::String (i + 1)).toRawUTF8(),
                    ("TVF Env Time " + juce::String (i + 1)).toRawUTF8(),
                    0x55 + (uint32_t) i, 0, 127);
        for (int i = 0; i < 5; ++i)
            add (B, ("tone.tvf_l" + juce::String (i)).toRawUTF8(),
                    ("TVF Env Level " + juce::String (i)).toRawUTF8(),
                    0x59 + (uint32_t) i, 0, 127);

        add (B, "tone.bias_level",   "Bias Level",       0x5E, 54, 74, 64, 10);
        add (B, "tone.bias_pos",     "Bias Position",    0x5F, 0, 127);
        add (B, "tone.bias_dir",     "Bias Direction",   0x60, 0, 3, 0, 1, biasDir);
        add (B, "tone.tva_vcrv",     "TVA Vel Curve",    0x61, 0, 7, 0, 1, velCurve);
        add (B, "tone.tva_vsens",    "TVA Vel Sens",     0x62, 1, 127, 64);
        add (B, "tone.tva_t1_vs",    "TVA T1 V-Sens",    0x63, 1, 127, 64);
        add (B, "tone.tva_t4_vs",    "TVA T4 V-Sens",    0x64, 1, 127, 64);
        add (B, "tone.tva_t_kf",     "TVA Time KF",      0x65, 54, 74, 64, 10);
        for (int i = 0; i < 4; ++i)
            add (B, ("tone.tva_t" + juce::String (i + 1)).toRawUTF8(),
                    ("TVA Env Time " + juce::String (i + 1)).toRawUTF8(),
                    0x66 + (uint32_t) i, 0, 127);
        for (int i = 0; i < 3; ++i)
            add (B, ("tone.tva_l" + juce::String (i + 1)).toRawUTF8(),
                    ("TVA Env Level " + juce::String (i + 1)).toRawUTF8(),
                    0x6A + (uint32_t) i, 0, 127);

        // LFO 1 lives at 0x6E..0x7A, LFO 2 straddles the 128-byte boundary.
        struct LfoLayout { uint32_t rate, offset, detune, delay, delayKf, fadeMode, fadeTime, keyTrig,
                                    pitchDep, tvfDep, tvaDep, panDep, morph; };
        const LfoLayout lfo1 { 0x6E, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, P1 + 0x09 };
        const LfoLayout lfo2 { 0x7C, 0x7E, 0x7F, P1 + 0x00, P1 + 0x01, P1 + 0x02, P1 + 0x03,
                               P1 + 0x04, P1 + 0x05, P1 + 0x06, P1 + 0x07, P1 + 0x08, P1 + 0x0A };

        const LfoLayout layouts[2] = { lfo1, lfo2 };
        for (int n = 0; n < 2; ++n)
        {
            const auto& L = layouts[n];
            const juce::String tag ("tone.lfo" + juce::String (n + 1) + ".");
            const juce::String pre ("LFO " + juce::String (n + 1) + " ");

            add (B, (tag + "rate").toRawUTF8(),      (pre + "Rate").toRawUTF8(),        L.rate, 0, 149, 0, 1, {}, 2);
            add (B, (tag + "offset").toRawUTF8(),    (pre + "Offset").toRawUTF8(),      L.offset, 0, 4, 0, 1, lfoOffset);
            add (B, (tag + "detune").toRawUTF8(),    (pre + "Rate Detune").toRawUTF8(), L.detune, 0, 127);
            add (B, (tag + "delay").toRawUTF8(),     (pre + "Delay Time").toRawUTF8(),  L.delay, 0, 127);
            add (B, (tag + "delay_kf").toRawUTF8(),  (pre + "Delay KF").toRawUTF8(),    L.delayKf, 54, 74, 64, 10);
            add (B, (tag + "fade_mode").toRawUTF8(), (pre + "Fade Mode").toRawUTF8(),   L.fadeMode, 0, 3, 0, 1, fadeMode);
            add (B, (tag + "fade_time").toRawUTF8(), (pre + "Fade Time").toRawUTF8(),   L.fadeTime, 0, 127);
            add (B, (tag + "key_trig").toRawUTF8(),  (pre + "Key Trigger").toRawUTF8(), L.keyTrig, 0, 1, 0, 1, offOn);
            add (B, (tag + "pitch_dep").toRawUTF8(), (pre + "Pitch Depth").toRawUTF8(), L.pitchDep, 1, 127, 64);
            add (B, (tag + "tvf_dep").toRawUTF8(),   (pre + "TVF Depth").toRawUTF8(),   L.tvfDep, 1, 127, 64);
            add (B, (tag + "tva_dep").toRawUTF8(),   (pre + "TVA Depth").toRawUTF8(),   L.tvaDep, 1, 127, 64);
            add (B, (tag + "pan_dep").toRawUTF8(),   (pre + "Pan Depth").toRawUTF8(),   L.panDep, 1, 127, 64);
            add (B, (tag + "morph").toRawUTF8(),     (pre + "Waveform").toRawUTF8(),    L.morph, 0, 127);
        }
    }

    // -----------------------------------------------------------------------
    void buildPartInfoPart()
    {
        const auto B = Block::partInfoPart;
        add (B, "part.rx_switch",  "Receive Switch",  0x00, 0, 1, 0, 1, offOn);
        add (B, "part.bank_msb",   "Bank Select MSB", 0x01, 0, 127);
        add (B, "part.bank_lsb",   "Bank Select LSB", 0x02, 0, 127);
        add (B, "part.program",    "Program Number",  0x03, 0, 127);
        add (B, "part.level",      "Part Level",      0x04, 0, 127);
        add (B, "part.pan",        "Part Pan",        0x05, 0, 127, 64);
        add (B, "part.coarse",     "Coarse Tune",     0x06, 16, 112, 64);
        add (B, "part.fine",       "Fine Tune",       0x07, 14, 114, 64);
        add (B, "part.dry_send",   "Dry Send Level",  0x08, 0, 127);
        add (B, "part.rev_send",   "Reverb Send",     0x09, 0, 127);
        add (B, "part.output",     "Output Select",   0x0A, 0, 6, 0, 1, outSelect);
        add (B, "part.auto_sync",  "Auto Sync",       0x0B, 0, 1, 0, 1, offOn);
    }

    void buildCompEQ()
    {
        const auto B = Block::compEQ;
        add (B, "comp.rev_send",  "Comp Reverb Send", 0x00, 0, 127);
        add (B, "comp.output",    "Comp Output",      0x01, 0, 2, 0, 1, compOutAsgn);
        add (B, "comp.attack",    "Attack",           0x02, 0, 31);
        add (B, "comp.release",   "Release",          0x03, 0, 23);
        add (B, "comp.gain",      "Output Gain",      0x04, 0, 24);
        add (B, "comp.threshold", "Threshold",        0x05, 0, 127);
        add (B, "comp.ratio",     "Ratio",            0x06, 0, 19);
        add (B, "comp.low_freq",  "Low Freq",         0x07, 0, 1, 0, 1, compLowFreq);
        add (B, "comp.low_gain",  "Low Gain",         0x08, 0, 30, 15);
        add (B, "comp.high_freq", "High Freq",        0x09, 0, 2, 0, 1, compHighFreq);
        add (B, "comp.high_gain", "High Gain",        0x0A, 0, 30, 15);
        add (B, "comp.level",     "Comp Level",       0x0B, 0, 127);
    }

    void buildSystem()
    {
        const auto B = Block::systemCommon;
        add (B, "sys.master_tune",  "Master Tune",       0x00, 24, 2024, 1024, 1, {}, 4);
        add (B, "sys.key_shift",    "Master Key Shift",  0x04, 40, 88, 64);
        add (B, "sys.master_level", "Master Level",      0x05, 0, 127);
        add (B, "sys.scale_tune",   "Scale Tune",        0x06, 0, 1, 0, 1, offOn);
        add (B, "sys.patch_remain", "Patch Remain",      0x07, 0, 1, 0, 1, offOn);
        add (B, "sys.rx_pc",        "Receive Prog Chg",  0x08, 0, 1, 0, 1, offOn);
        add (B, "sys.rx_bank",      "Receive Bank Sel",  0x09, 0, 1, 0, 1, offOn);

        const auto M = Block::mastering;
        add (M, "mst.switch", "Mastering Switch", 0x00, 0, 1, 0, 1, offOn);
        const char* bandNames[3] = { "Low", "Mid", "High" };
        for (int band = 0; band < 3; ++band)
        {
            const uint32_t base = 0x01 + (uint32_t) band * 5;
            const juce::String pre (juce::String (bandNames[band]) + " ");
            const juce::String tag ("mst." + juce::String (bandNames[band]).toLowerCase() + ".");

            add (M, (tag + "attack").toRawUTF8(),    (pre + "Attack").toRawUTF8(),    base + 0, 0, 100);
            add (M, (tag + "release").toRawUTF8(),   (pre + "Release").toRawUTF8(),   base + 1, 0, 100);
            add (M, (tag + "threshold").toRawUTF8(), (pre + "Threshold").toRawUTF8(), base + 2, 0, 36, 36);
            add (M, (tag + "ratio").toRawUTF8(),     (pre + "Ratio").toRawUTF8(),     base + 3, 0, 13);
            add (M, (tag + "level").toRawUTF8(),     (pre + "Level").toRawUTF8(),     base + 4, 0, 24);
        }
        add (M, "mst.split_low",  "Split Freq Low",  0x10, 0, 6, 0, 1,
             SA { "200", "250", "315", "400", "500", "630", "800" });
        add (M, "mst.split_high", "Split Freq High", 0x11, 0, 6, 0, 1,
             SA { "2000", "2500", "3150", "4000", "5000", "6300", "8000" });
    }

    void buildAll()
    {
        gTable.clear();
        buildPatchCommon();
        buildPatchTMT();
        buildPatchTone();
        buildPartInfoPart();
        buildCompEQ();
        buildSystem();
    }
} // anonymous namespace

const std::vector<ParamDef>& allParams()
{
    static bool built = false;
    if (! built) { buildAll(); built = true; }
    return gTable;
}

std::vector<const ParamDef*> paramsForBlock (Block b)
{
    std::vector<const ParamDef*> out;
    for (const auto& p : allParams())
        if (p.block == b)
            out.push_back (&p);
    return out;
}

uint32_t blockByteCount (Block b)
{
    switch (b)
    {
        case Block::patchCommon:   return kSizePatchCommon;
        case Block::patchTMT:      return kSizePatchTMT;
        case Block::patchTone:     return kSizePatchTone;
        case Block::partInfoPart:  return kSizePartInfoPart;
        case Block::compEQ:        return kSizeCompEQ;
        case Block::systemCommon:  return kSizeSystemCommon;
        case Block::mastering:     return kSizeMastering;
    }
    return 0;
}

roland::Address blockAddress (Block b, int part, int tone)
{
    part = juce::jlimit (0, 15, part);
    tone = juce::jlimit (0, 3, tone);

    switch (b)
    {
        case Block::patchCommon:
            return temporaryPart (part) + (kTempPatch + kPatchCommon);
        case Block::patchTMT:
            return temporaryPart (part) + (kTempPatch + kPatchTMT);
        case Block::patchTone:
            return temporaryPart (part) + (kTempPatch + kPatchTone0 + (uint32_t) tone * kPatchToneStride);
        case Block::partInfoPart:
            return partInfoBase() + (kPartInfoPart0 + (uint32_t) part * kPartInfoPartStride);
        case Block::compEQ:
            return partInfoBase() + kPartInfoCompEQ;
        case Block::systemCommon:
            return systemBase() + kSystemCommon;
        case Block::mastering:
            return systemBase() + kSystemMastering;
    }
    return {};
}

namespace quick
{
    const std::vector<AutoParam>& automatable()
    {
        static const std::vector<AutoParam> v {
            { "auto.cutoff",     "Cutoff",         kCutoff      },
            { "auto.resonance",  "Resonance",      kResonance   },
            { "auto.tvf_env",    "TVF Env Depth",  kTvfEnvDepth },
            { "auto.tvf_atk",    "TVF Attack",     kTvfAttack   },
            { "auto.tvf_rel",    "TVF Release",    kTvfRelease  },
            { "auto.tva_atk",    "Amp Attack",     kTvaAttack   },
            { "auto.tva_rel",    "Amp Release",    kTvaRelease  },
            { "auto.lfo_rate",   "LFO Rate",       kLfoRate     },
            { "auto.lfo_pitch",  "LFO Pitch Depth", kLfoPitchDep },
            { "auto.lfo_tvf",    "LFO Filter Depth", kLfoTvfDep },
            { "auto.lfo_tva",    "LFO Amp Depth",  kLfoTvaDep   },
            { "auto.pan",        "Pan",            kPan         },
        };
        return v;
    }
}

} // namespace mc909
