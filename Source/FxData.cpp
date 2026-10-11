#include "FxData.h"
#include <map>

namespace fxdata
{
namespace
{
    // Field syntax:  Label#lo#hi#unit   integer range
    //                Label#=#a/b/c      choice
    //                Label#~#text       free value, text is the range printed in the manual
    // Tokens starting with @ are shorthand for parameters that repeat in many effects.
    const std::map<juce::String, juce::String>& tokens()
    {
        static const std::map<juce::String, juce::String> t =
        {
            { "@LG",    "Low Gain#-15#15#dB" },
            { "@HG",    "High Gain#-15#15#dB" },
            { "@LV",    "Level#0#127#" },
            { "@PAN",   "Pan#-64#63#" },
            { "@BAL",   "Balance#~#D100:0W to D0:100W" },
            { "@RATE",  "Rate#~#0.05-10.00 Hz or note" },
            { "@RATEH", "Rate#~#0.05-10.00 Hz" },
            { "@PRED",  "Pre Delay#~#0.0-100.0 ms" },
            { "@FB",    "Feedback#-98#98#%" },
            { "@HFD",   "HF Damp#~#200-8000 Hz or BYPASS" },
            { "@DEPTH", "Depth#0#127#" },
            { "@OFFON", "#=#OFF/ON" },
            { "@D4",    "Delay 1#~#0-4000 ms or note;Delay 2#~#0-4000 ms or note;Delay 3#~#0-4000 ms or note;Delay 4#~#0-4000 ms or note" },
            { "@BEATS", "Beat 1-1#0#127#;Beat 1-2#0#127#;Beat 1-3#0#127#;Beat 1-4#0#127#;Beat 2-1#0#127#;Beat 2-2#0#127#;Beat 2-3#0#127#;Beat 2-4#0#127#;"
                        "Beat 3-1#0#127#;Beat 3-2#0#127#;Beat 3-3#0#127#;Beat 3-4#0#127#;Beat 4-1#0#127#;Beat 4-2#0#127#;Beat 4-3#0#127#;Beat 4-4#0#127#" },
        };
        return t;
    }

    const char* const mfxSpec[] =
    {
        "STEREO EQ|Low Freq#=#200 Hz/400 Hz;@LG;High Freq#=#2000 Hz/4000 Hz/8000 Hz;@HG;Mid1 Freq#~#200-8000 Hz;Mid1 Q#=#0.5/1.0/2.0/4.0/8.0;Mid1 Gain#-15#15#dB;Mid2 Freq#~#200-8000 Hz;Mid2 Q#=#0.5/1.0/2.0/4.0/8.0;Mid2 Gain#-15#15#dB;@LV",
        "SPECTRUM|Q#=#0.5/1.0/2.0/4.0/8.0;@PAN;@LV;Band 1 (250Hz)#-15#15#dB;Band 2 (500Hz)#-15#15#dB;Band 3 (1kHz)#-15#15#dB;Band 4 (1.25kHz)#-15#15#dB;Band 5 (2kHz)#-15#15#dB;Band 6 (3.15kHz)#-15#15#dB;Band 7 (4kHz)#-15#15#dB;Band 8 (8kHz)#-15#15#dB",
        "ENHANCER|Sens#0#127#;Mix#0#127#;@LG;@HG;@LV",
        "ISOLATOR|Boost/Cut High#-60#4#dB;Boost/Cut Middle#-60#4#dB;Boost/Cut Low#-60#4#dB;AntiPhase Middle Sw#=#OFF/ON;AntiPhase Middle Level#0#127#;Anti Phase Low Sw#=#OFF/ON;Anti Phase Low Level#0#127#;Low Boost Sw#=#OFF/ON;Low Boost Level#0#127#;@LV",
        "LOW BOOST|Boost Frequency#~#50-125 Hz;Boost Gain#0#12#dB;Boost Width#=#WIDE/MID/NARROW;@LG;@HG;@LV",
        "SUPER FILTER|Filter Type#=#LPF/BPF/HPF/NOTCH;Filter Slope#=#-12 dB/-24 dB/-36 dB;Filter Cutoff#0#127#;Filter Resonance#0#127#;Filter Gain#0#12#dB;Modulation Sw#=#OFF/ON;Modulation Wave#=#TRI/SQU/SIN/SAW1/SAW2;@RATE;@DEPTH;Attack#0#127#;@LV",
        "STEP FILTER|@RATE;Attack#0#127#;Filter Type#=#LPF/BPF/HPF/NOTCH;Filter Slope#=#-12 dB/-24 dB/-36 dB;Filter Resonance#0#127#;Filter Gain#0#12#dB;@LV;@BEATS",
        "AUTO WAH|Filter Type#=#LPF/BPF;@RATE;@DEPTH;Sens#0#127#;Manual#0#127#;Peak#0#127#;@LV",
        "HUMANIZER|Drive Sw#=#OFF/ON;Drive#0#127#;Vowel1#=#a/e/i/o/u;Vowel2#=#a/e/i/o/u;@RATE;@DEPTH;Input Sync Sw#=#OFF/ON;Input Sync Threshold#0#127#;Manual#0#100#;@LG;@HG;@PAN;@LV",
        "PHASER|Manual#0#127#;@RATEH;@DEPTH;Resonance#0#127#;Mix#0#127#;@PAN;@LV",
        "STEREO PHASER|Mode#=#4 stage/8 stage;Polarity#=#INVERSE/SYNCHRO;@RATE;@DEPTH;Manual#0#127#;Resonance#0#127#;Cross Feedback#-98#98#%;Mix#0#127#;@LG;@HG;@LV",
        "STEP PHASER|Mode#=#4 stage/8 stage;Polarity#=#INVERSE/SYNCHRO;@RATE;@DEPTH;Manual#0#127#;Resonance#0#127#;Cross Feedback#-98#98#%;Step Rate#~#0.1-20.0 Hz or note;Mix#0#127#;@LG;@HG;@LV",
        "RING MODULATOR|Frequency#0#127#;Sens#0#127#;Polarity#=#UP/DOWN;@LG;@HG;@BAL;@LV",
        "TREMOLO|Modulation Wave#=#TRI/SQR/SIN/SAW1/SAW2;@RATE;@DEPTH;@LG;@HG;@LV",
        "AUTO PAN|Modulation Wave#=#TRI/SQR/SIN/SAW1/SAW2;@RATE;@DEPTH;@LG;@HG;@LV",
        "ROTARY|Tweeter Slow Rate#~#0.05-10.00 Hz;Woofer Slow Rate#~#0.05-10.00 Hz;Tweeter Fast Rate#~#0.05-10.00 Hz;Woofer Fast Rate#~#0.05-10.00 Hz;Speed#=#SLOW/FAST;Tweeter Acceleration#0#15#;Woofer Acceleration#0#15#;Tweeter Level#0#127#;Woofer Level#0#127#;Separation#0#127#;@LV",
        "HEXA-CHORUS|@PRED;@RATEH;@DEPTH;Pre Delay Deviation#0#20#;Depth Deviation#-20#20#;Pan Deviation#0#20#;@BAL;@LV",
        "TREMOLO CHORUS|@PRED;Chorus Rate#~#0.05-10.00 Hz;Chorus Depth#0#127#;Tremolo Rate#~#0.05-10.00 Hz;Tremolo Separation#0#127#;Tremolo Phase#0#180#deg;@BAL;@LV",
        "SPACE-D|@PRED;@RATEH;@DEPTH;Phase#0#180#deg;@LG;@HG;@BAL;@LV",
        "STEREO CHORUS|Filter Type#=#OFF/LPF/HPF;Cutoff Freq#~#200-8000 Hz;@PRED;@RATEH;@DEPTH;Phase#0#180#deg;@LG;@HG;@BAL;@LV",
        "STEREO FLANGER|Filter Type#=#OFF/LPF/HPF;Cutoff Freq#~#200-8000 Hz;@PRED;@RATE;@DEPTH;Phase#0#180#deg;@FB;@LG;@HG;@BAL;@LV",
        "STEP FLANGER|@PRED;@RATE;@DEPTH;@FB;Step Rate#~#0.10-20.00 Hz or note;Phase#0#180#deg;@LG;@HG;@BAL;@LV",
        "OVERDRIVE|Drive#0#127#;Tone#0#127#;@PAN;Amp Sw#=#OFF/ON;Amp Type#=#SMALL/BUILT-IN/2-STACK/3-STACK;@LG;@HG;@LV",
        "DISTORTION|Drive#0#127#;Tone#0#127#;@PAN;Amp Sw#=#OFF/ON;Amp Type#=#SMALL/BUILT-IN/2-STACK/3-STACK;@LG;@HG;@LV",
        "GUITAR AMP SIMULATOR|Pre Amp Sw#=#OFF/ON;Pre Amp Type#=#JC-120/Clean Twin/Match Drive/BG Lead/MS1959I/MS1959II/MS1959I+II/SLDN Lead/Metal 5150/Metal Lead/OD-1/OD-2 TURBO/Distortion/Fuzz;Pre Amp Volume#0#127#;Pre Amp Master#0#127#;Pre Amp Gain#=#Low/Middle/High;Pre Amp Bass#0#127#;Pre Amp Middle#0#127#;Pre Amp Treble#0#127#;Pre Amp Presence#0#127#;Pre Amp Bright#=#OFF/ON;Speaker Sw#=#OFF/ON;Speaker Type#=#Small1/Small2/Middle/JC-120/Built In 1/Built In 2/Built In 3/Built In 4/Built In 5/BG Stack 1/BG Stack 2/MS Stack1/MS Stack 2/Metal Stack/2 Stack/3 Stack;Mic Setting#=#1/2/3;Mic Level#0#127#;Direct Level#0#127#;@PAN;@LV",
        "COMPRESSOR|Attack#0#127#;Threshold#0#127#;Post Gain#=#0 dB/+6 dB/+12 dB/+18 dB;@LG;@HG;@LV",
        "LIMITER|Release#0#127#;Threshold#0#127#;Ratio#=#1.5:1/2:1/4:1/100:1;Post Gain#=#0 dB/+6 dB/+12 dB/+18 dB;@LG;@HG;@LV",
        "SLICER|@RATE;Attack#0#127#;Input Sync Sw#=#OFF/ON;Input Sync Threshold#0#127#;Mode#=#LEGATO/SLASH;Shuffle#0#127#;@LV;@BEATS",
        "GATE|Threshold#0#127#;Mode#=#GATE/DUCK;@BAL;Attack Time#0#127#;Hold Time#0#127#;Release Time#0#127#;@LV",
        "LOFI NOISE|LoFi Type#1#9#;Post Filter Type#=#OFF/LPF/HPF;Post Filter Cutoff#~#200-8000 Hz;W/P Noise Type#=#WHITE/PINK;W/P Noise LPF#~#200-8000 Hz or BYPASS;W/P Noise Level#0#127#;Disc Noise Type#=#LP/EP/SP/RND;Disc Noise LPF#~#200-8000 Hz or BYPASS;Disc Noise Level#0#127#;Hum Noise Type#=#50 Hz/60 Hz;Hum Noise LPF#~#200-8000 Hz or BYPASS;Hum Noise Level#0#127#;@BAL;@LV",
        "LOFI COMPRESS|Pre Filter Type#1#6#;LoFi Type#1#9#;Post Filter Type#=#OFF/LPF/HPF;Post Filter Cutoff#~#200-8000 Hz;@BAL;@LV",
        "LOFI RADIO|LoFi Type#1#9#;Post Filter Type#=#OFF/LPF/HPF;Post Filter Cutoff#~#200-8000 Hz;Radio Detune#0#127#;Radio Noise Level#0#127#;@BAL;@LV",
        "TELEPHONE|Voice Quality#0#15#;Treble#-15#15#dB;@BAL;@LV",
        "PHONOGRAPH|Signal Distortion#0#127#;Frequency Range#0#127#;Disc Type#=#LP/EP/SP;Scratch Noise Level#0#127#;Dust Noise Level#0#127#;Hiss Noise Level#0#127#;Total Noise Level#0#127#;Wow#0#127#;Flutter#0#127#;Random#0#127#;Total Wow/Flutter#0#127#;@BAL;@LV",
        "TAPE ECHO|Mode#=#S/M/L/S+M/S+L/M+L/S+M+L;Repeat Rate#0#127#;Intensity#0#127#;Bass#-15#15#dB;Treble#-15#15#dB;Head S Pan#-64#63#;Head M Pan#-64#63#;Head L Pan#-64#63#;Tape Distortion#0#5#;Wow/Flutter Rate#0#127#;Wow/Flutter Depth#0#127#;Echo Level#0#127#;Direct Level#0#127#",
        "FBK PITCH SHIFTER|Mode#1#5#;Coarse#-24#12#semi;Fine#-100#100#cent;Pre Delay#~#0.0-500 ms;Feedback#-98#98#%;@PAN;@LG;@HG;@BAL;@LV",
        "2Vo PITCH SHIFTER|Mode#1#5#;Coarse A#-24#12#semi;Coarse B#-24#12#semi;Fine A#-100#100#cent;Fine B#-100#100#cent;Pre Delay A#~#0.0-500 ms;Pre Delay B#~#0.0-500 ms;Pan A#-64#63#;Pan B#-64#63#;Level Balance#~#A100:0B to A0:100B;@BAL;@LV",
        "GATED REVERB|Type#=#NORMAL/REVERSE;@PRED;Time#~#5-500 ms;Pan#-64#63#;@LG;@HG;@BAL;@LV",
        "STEREO DELAY|Feedback Mode#=#NORMAL/CROSS;Delay Left#~#0-2000 ms or note;Delay Right#~#0-2000 ms or note;Phase Left#=#NORMAL/INVERT;Phase Right#=#NORMAL/INVERT;@FB;@HFD;@LG;@HG;@BAL;@LV",
        "MODULATION DELAY|Feedback Mode#=#NORMAL/CROSS;Delay Left#~#0-2000 ms or note;Delay Right#~#0-2000 ms or note;@FB;@HFD;@RATEH;@DEPTH;Phase#0#180#deg;@LG;@HG;@BAL;@LV",
        "TRIPLE TAP DELAY|Delay Left#~#0-4000 ms or note;Delay Right#~#0-4000 ms or note;Delay Center#~#0-4000 ms or note;@FB;@HFD;Left Level#0#127#;Right Level#0#127#;Center Level#0#127#;@LG;@HG;@BAL;@LV",
        "QUADRUPLE TAP DELAY|@D4;Level 1#0#127#;Level 2#0#127#;Level 3#0#127#;Level 4#0#127#;@FB;@HFD;@BAL;@LV",
        "MULTI TAP DELAY|@D4;Pan 1#-64#63#;Pan 2#-64#63#;Pan 3#-64#63#;Pan 4#-64#63#;Level 1#0#127#;Level 2#0#127#;Level 3#0#127#;Level 4#0#127#;@FB;@HFD;@LG;@HG;@BAL;@LV",
        "REVERSE DELAY|Threshold#0#127#;Delay 1#~#0-2000 ms or note;Delay 2#~#0-2000 ms or note;Delay 3#~#0-2000 ms or note;Delay 4#~#0-2000 ms or note;Feedback 1#-98#98#%;Feedback 4#-98#98#%;HF Damp 1#~#200-8000 Hz or BYPASS;HF Damp 4#~#200-8000 Hz or BYPASS;Pan 1#-64#63#;Pan 2#-64#63#;Pan 3#-64#63#;Level 1#0#127#;Level 2#0#127#;Level 3#0#127#;@BAL;@LG;@HG;@LV",
        "SHUFFLE DELAY|Delay#~#0-4000 ms or note;Shuffle Rate#0#100#%;Pan A#-64#63#;Pan B#-64#63#;Level Balance#~#A100:0B to A0:100B;@FB;Acceleration#0#15#;@HFD;@LG;@HG;@BAL;@LV",
        "TIME CONTROL DELAY|Delay#~#0-4000 ms or note;@FB;Acceleration#0#15#;@HFD;@PAN;@LG;@HG;@BAL;@LV",
        "TIME SKIP DELAY|Delay#~#0-4000 ms or note;Skip Rate#~#0.05-10.0 Hz or note;@FB;Acceleration#0#15#;@HFD;@PAN;@LG;@HG;@BAL;@LV",
    };

    const char* const reverbSpec[] =
    {
        "Reverb|Type#=#ROOM1/ROOM2/STAGE1/STAGE2/HALL1/HALL2/DELAY/PAN-DELAY;Time#0#127#;HF Damp#~#200-8000 Hz or BYPASS;Delay Feedback#0#127#;Level#0#127#",
        "SRV Room|Pre Delay#~#0.0-100.0 ms;Time#0#127#;Size#1#8#;High Cut#~#160-12500 Hz or BYPASS;Density#0#127#;Diffusion#0#127#;LF Damp Freq#~#50-4000 Hz;LF Damp Gain#-36#0#dB;HF Damp Freq#~#4000-12500 Hz;HF Damp Gain#-36#0#dB;Level#0#127#",
        "SRV Hall|Pre Delay#~#0.0-100.0 ms;Time#0#127#;Size#1#8#;High Cut#~#160-12500 Hz or BYPASS;Density#0#127#;Diffusion#0#127#;LF Damp Freq#~#50-4000 Hz;LF Damp Gain#-36#0#dB;HF Damp Freq#~#4000-12500 Hz;HF Damp Gain#-36#0#dB;Level#0#127#",
        "SRV Plate|Pre Delay#~#0.0-100.0 ms;Time#0#127#;Size#1#8#;High Cut#~#160-12500 Hz or BYPASS;Density#0#127#;Diffusion#0#127#;LF Damp Freq#~#50-4000 Hz;LF Damp Gain#-36#0#dB;HF Damp Freq#~#4000-12500 Hz;HF Damp Gain#-36#0#dB;Level#0#127#",
    };

    std::vector<Param> parseParams (juce::String body)
    {
        // Longest keys first so "@RATEH" is not eaten by "@RATE".
        for (auto it = tokens().rbegin(); it != tokens().rend(); ++it)
            body = body.replace (it->first, it->second);

        std::vector<Param> out;
        for (const auto& item : juce::StringArray::fromTokens (body, ";", ""))
        {
            const auto f = juce::StringArray::fromTokens (item, "#", "");
            if (f.size() < 3)
                continue;

            Param p;
            p.label = f[0];

            if (f[1] == "~")
            {
                p.kind = Param::Kind::free;
                p.hint = f[2];
            }
            else if (f[1] == "=")
            {
                p.kind = Param::Kind::choice;
                p.options = juce::StringArray::fromTokens (f[2], "/", "");
                p.lo = 0;
                p.hi = p.options.size() - 1;
            }
            else
            {
                p.kind = Param::Kind::integer;
                p.lo = f[1].getIntValue();
                p.hi = f[2].getIntValue();
                p.unit = f.size() > 3 ? f[3] : juce::String();
            }
            out.push_back (std::move (p));
        }
        return out;
    }

    struct Tables
    {
        juce::StringArray mfxNames, revNames;
        std::vector<std::vector<Param>> mfx, rev;
        std::vector<Param> none;

        Tables()
        {
            mfxNames.add ("THRU");
            mfx.emplace_back();
            for (auto* s : mfxSpec)
            {
                const juce::String line (s);
                mfxNames.add (line.upToFirstOccurrenceOf ("|", false, false));
                mfx.push_back (parseParams (line.fromFirstOccurrenceOf ("|", false, false)));
            }

            revNames.add ("OFF");
            rev.emplace_back();
            for (auto* s : reverbSpec)
            {
                const juce::String line (s);
                revNames.add (line.upToFirstOccurrenceOf ("|", false, false));
                rev.push_back (parseParams (line.fromFirstOccurrenceOf ("|", false, false)));
            }
        }
    };

    const Tables& T() { static const Tables t; return t; }
}

const juce::StringArray& mfxTypeNames()    { return T().mfxNames; }
const juce::StringArray& reverbTypeNames() { return T().revNames; }

const std::vector<Param>& mfxParams (int type)
{
    return juce::isPositiveAndBelow (type, (int) T().mfx.size()) ? T().mfx[(size_t) type] : T().none;
}

const std::vector<Param>& reverbParams (int type)
{
    return juce::isPositiveAndBelow (type, (int) T().rev.size()) ? T().rev[(size_t) type] : T().none;
}

juce::String labelHelp (const juce::String& label)
{
    static const std::map<juce::String, juce::String> h =
    {
        { "Level",      "Output level of the effect." },
        { "Pan",        "Stereo position of the effect output." },
        { "Low Gain",   "Boost or cut of the low frequency range." },
        { "High Gain",  "Boost or cut of the high frequency range." },
        { "Balance",    "Mix between the direct sound (D) and the effect sound (W)." },
        { "Rate",       "Speed of the modulation." },
        { "Depth",      "Depth of the modulation." },
        { "Pre Delay",  "Time before the effect sound is heard." },
        { "Phase",      "Spatial spread of the sound." },
        { "Feedback",   "How much of the effect sound is fed back into the effect. Negative values invert the phase." },
        { "Manual",     "Basic frequency from which the sound is modulated." },
        { "Resonance",  "Amount of feedback or emphasis around the centre frequency." },
        { "Mix",        "Level of the processed sound." },
        { "HF Damp",    "Frequency above which the fed-back sound is filtered out. BYPASS means no filtering." },
        { "Filter Type","Type of filter: which part of the spectrum passes." },
        { "Cutoff Freq","Basic frequency of the filter." },
        { "Drive",      "Amount of distortion. It also changes the volume." },
        { "Tone",       "Sound quality (tone) of the effect." },
        { "Amp Sw",     "Turns the amp simulator on or off." },
        { "Amp Type",   "Type of guitar amp being simulated." },
        { "Threshold",  "Volume level at which the effect begins to act." },
        { "Attack",     "Speed at which the effect responds." },
        { "Release",    "Time the effect takes to stop acting once the sound falls below the threshold." },
        { "Time",       "Length of the effect (reverb time or delay time, depending on the effect)." },
        { "Size",       "Size of the simulated room or hall, 1 to 8." },
        { "Density",    "Density of the reverb." },
        { "Diffusion",  "How the reverb density changes over time. Higher values build density as time passes." },
        { "Acceleration","How quickly the delay time moves to a newly set value." },
        { "Mode",       "Operating mode of the effect. See the Owner's Manual for the effect." },
        { "Polarity",   "Whether the left and right modulation are opposite (INVERSE) or the same (SYNCHRO)." },
        { "Modulation Wave", "Shape of the modulation: triangle, square, sine, or two sawtooth shapes." },
        { "Cross Feedback",  "Amount of phased sound fed back across channels. Negative values invert the phase." },
    };

    if (auto it = h.find (label); it != h.end())
        return it->second;

    return {};
}

} // namespace fxdata
