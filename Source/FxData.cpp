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
            { "@LG",    "Low Gain#$g15#0#30" },
            { "@HG",    "High Gain#$g15#0#30" },
            { "@LV",    "Level#0#127#" },
            { "@PAN",   "Pan#$pan#0#127" },
            { "@BAL",   "Balance#$bal#0#100" },
            { "@RATE",  "Rate#$r05#0#200" },
            { "@RATEH", "Rate#$r05#0#200" },
            { "@PRED",  "Pre Delay#$pre#0#125" },
            { "@FB",    "Feedback#$fb#0#98" },
            { "@HFD",   "HF Damp#$hzb#0#17" },
            { "@DEPTH", "Depth#0#127#" },
            { "@OFFON", "#=#OFF/ON" },
            { "@D4",    "Delay 1#$delay4#1#4022;Delay 2#$delay4#1#4022;Delay 3#$delay4#1#4022;Delay 4#$delay4#1#4022" },
            { "@STEPS", "Step 1#0#127#;Step 2#0#127#;Step 3#0#127#;Step 4#0#127#;Step 5#0#127#;Step 6#0#127#;Step 7#0#127#;Step 8#0#127#;Step 9#0#127#;Step 10#0#127#;Step 11#0#127#;Step 12#0#127#;Step 13#0#127#;Step 14#0#127#;Step 15#0#127#;Step 16#0#127#" }
        };
        return t;
    }

    const char* const mfxSpec[] =
    {
        "STEREO EQ|Low Freq#=#200 Hz/400 Hz;@LG;High Freq#=#2000 Hz/4000 Hz/8000 Hz;@HG;Mid1 Freq#$hz#0#16;Mid1 Q#=#0.5/1.0/2.0/4.0/8.0;Mid1 Gain#$g15#0#30;Mid2 Freq#$hz#0#16;Mid2 Q#=#0.5/1.0/2.0/4.0/8.0;Mid2 Gain#$g15#0#30;@LV",
        "SPECTRUM|Band 1 (250Hz)#$g15#0#30;Band 2 (500Hz)#$g15#0#30;Band 3 (1kHz)#$g15#0#30;Band 4 (1.25kHz)#$g15#0#30;Band 5 (2kHz)#$g15#0#30;Band 6 (3.15kHz)#$g15#0#30;Band 7 (4kHz)#$g15#0#30;Band 8 (8kHz)#$g15#0#30;Q#=#0.5/1.0/2.0/4.0/8.0;@PAN;@LV",
        "ENHANCER|Sens#0#127#;Mix#0#127#;@LG;@HG;@LV",
        "ISOLATOR|Boost/Cut High#$g60#0#64;Boost/Cut Middle#$g60#0#64;Boost/Cut Low#$g60#0#64;AntiPhase Middle Sw#=#OFF/ON;AntiPhase Middle Level#0#127#;Anti Phase Low Sw#=#OFF/ON;Anti Phase Low Level#0#127#;Low Boost Sw#=#OFF/ON;Low Boost Level#0#127#;@LV",
        "LOW BOOST|Boost Frequency#$hzbo#0#8;Boost Gain#0#12#dB;Boost Width#=#WIDE/MID/NARROW;@LG;@HG;@LV",
        "SUPER FILTER|Filter Type#=#LPF/BPF/HPF/NOTCH;Filter Slope#=#-12 dB/-24 dB/-36 dB;Filter Cutoff#0#127#;Filter Resonance#0#127#;Filter Gain#0#12#dB;Modulation Sw#=#OFF/ON;Modulation Wave#=#TRI/SQU/SIN/SAW1/SAW2;@RATE;@DEPTH;Attack#0#127#;@LV",
        "STEP FILTER|@STEPS;@RATE;Attack#0#127#;Filter Type#=#LPF/BPF/HPF/NOTCH;Filter Slope#=#-12 dB/-24 dB/-36 dB;Filter Resonance#0#127#;Filter Gain#0#12#dB;@LV",
        "AUTO WAH|Filter Type#=#LPF/BPF;@RATE;@DEPTH;Sens#0#127#;Manual#0#127#;Peak#0#127#;@LV",
        "HUMANIZER|Drive Sw#=#OFF/ON;Drive#0#127#;Vowel1#=#a/e/i/o/u;Vowel2#=#a/e/i/o/u;@RATE;@DEPTH;Input Sync Sw#=#OFF/ON;Input Sync Threshold#0#127#;Manual#0#100#;@LG;@HG;@PAN;@LV",
        "PHASER|Manual#0#127#;@RATEH;@DEPTH;Resonance#0#127#;Mix#0#127#;@PAN;@LV",
        "STEREO PHASER|Mode#=#4 stage/8 stage/12 stage;Polarity#=#INVERSE/SYNCHRO;@RATE;@DEPTH;Manual#0#127#;Resonance#0#127#;Cross Feedback#$fb#0#98;Mix#0#127#;@LG;@HG;@LV",
        "STEP PHASER|Mode#=#4 stage/8 stage/12 stage;Polarity#=#INVERSE/SYNCHRO;@RATE;@DEPTH;Manual#0#127#;Resonance#0#127#;Cross Feedback#$fb#0#98;Step Rate#$r10#0#200;Mix#0#127#;@LG;@HG;@LV",
        "RING MODULATOR|Frequency#0#127#;Sens#0#127#;Polarity#=#UP/DOWN;@LG;@HG;@BAL;@LV",
        "TREMOLO|Modulation Wave#=#TRI/SQR/SIN/SAW1/SAW2;@RATE;@DEPTH;@LG;@HG;@LV",
        "AUTO PAN|Modulation Wave#=#TRI/SQR/SIN/SAW1/SAW2;@RATE;@DEPTH;@LG;@HG;@LV",
        "ROTARY|Tweeter Slow Rate#$r05#0#200;Woofer Slow Rate#$r05#0#200;Tweeter Fast Rate#$r05#0#200;Woofer Fast Rate#$r05#0#200;Speed#=#SLOW/FAST;Tweeter Acceleration#0#15#;Woofer Acceleration#0#15#;Tweeter Level#0#127#;Woofer Level#0#127#;Separation#0#127#;@LV",
        "HEXA-CHORUS|@PRED;@RATEH;@DEPTH;Pre Delay Deviation#0#20#;Depth Deviation#$off20#0#40;Pan Deviation#0#20#;@BAL;@LV",
        "TREMOLO CHORUS|@PRED;Chorus Rate#$r05#0#200;Chorus Depth#0#127#;Tremolo Rate#$r05#0#200;Tremolo Separation#0#127#;Tremolo Phase#$x2#0#90;@BAL;@LV",
        "SPACE-D|@PRED;@RATEH;@DEPTH;Phase#$x2#0#90;@LG;@HG;@BAL;@LV",
        "STEREO CHORUS|Filter Type#=#OFF/LPF/HPF;Cutoff Freq#$hz#0#16;@PRED;@RATEH;@DEPTH;Phase#$x2#0#90;-#~#unused slot;@LG;@HG;@BAL;@LV",
        "STEREO FLANGER|Filter Type#=#OFF/LPF/HPF;Cutoff Freq#$hz#0#16;@PRED;@RATE;@DEPTH;Phase#$x2#0#90;@FB;@LG;@HG;@BAL;@LV",
        "STEP FLANGER|@PRED;@RATE;@DEPTH;@FB;Step Rate#$r10#0#200;Phase#$x2#0#90;@LG;@HG;@BAL;@LV",
        "OVERDRIVE|Drive#0#127#;Tone#0#127#;@PAN;Amp Sw#=#OFF/ON;Amp Type#=#SMALL/BUILT-IN/2-STACK/3-STACK;@LG;@HG;@LV",
        "DISTORTION|Drive#0#127#;Tone#0#127#;@PAN;Amp Sw#=#OFF/ON;Amp Type#=#SMALL/BUILT-IN/2-STACK/3-STACK;@LG;@HG;@LV",
        "GUITAR AMP SIMULATOR|Pre Amp Sw#=#OFF/ON;Pre Amp Type#=#JC-120/Clean Twin/Match Drive/BG Lead/MS1959I/MS1959II/MS1959I+II/SLDN Lead/Metal 5150/Metal Lead/OD-1/OD-2 TURBO/Distortion/Fuzz;Pre Amp Volume#0#127#;Pre Amp Master#0#127#;Pre Amp Gain#=#Low/Middle/High;Pre Amp Bass#0#127#;Pre Amp Middle#0#127#;Pre Amp Treble#0#127#;Pre Amp Presence#$off127#0#127;Pre Amp Bright#=#OFF/ON;Speaker Sw#=#OFF/ON;Speaker Type#=#Small1/Small2/Middle/JC-120/Built In 1/Built In 2/Built In 3/Built In 4/Built In 5/BG Stack 1/BG Stack 2/MS Stack1/MS Stack 2/Metal Stack/2 Stack/3 Stack;Mic Setting#=#1/2/3;Mic Level#0#127#;Direct Level#0#127#;@PAN;@LV",
        "COMPRESSOR|Attack#0#127#;Threshold#0#127#;Post Gain#=#0 dB/+6 dB/+12 dB/+18 dB;@LG;@HG;@LV",
        "LIMITER|Release#0#127#;Threshold#0#127#;Ratio#=#1.5:1/2:1/4:1/100:1;Post Gain#=#0 dB/+6 dB/+12 dB/+18 dB;@LG;@HG;@LV",
        "SLICER|@STEPS;@RATE;Attack#0#127#;Input Sync Sw#=#OFF/ON;Input Sync Threshold#0#127#;Mode#=#LEGATO/SLASH;Shuffle#0#127#;@LV",
        "GATE|Threshold#0#127#;Mode#=#GATE/DUCK;@BAL;Attack Time#0#127#;Hold Time#0#127#;Release Time#0#127#;@LV",
        "LOFI NOISE|LoFi Type#$plus1#0#8;Post Filter Type#=#OFF/LPF/HPF;Post Filter Cutoff#$hz#0#16;W/P Noise Type#=#WHITE/PINK;W/P Noise LPF#$hzb#0#17;W/P Noise Level#0#127#;Disc Noise Type#=#LP/EP/SP/RND;Disc Noise LPF#$hzb#0#17;Disc Noise Level#0#127#;Hum Noise Type#=#50 Hz/60 Hz;Hum Noise LPF#$hzb#0#17;Hum Noise Level#0#127#;@BAL;@LV",
        "LOFI COMPRESS|Pre Filter Type#$plus1#0#5;LoFi Type#$plus1#0#8;Post Filter Type#=#OFF/LPF/HPF;Post Filter Cutoff#$hz#0#16;@BAL;@LV",
        "LOFI RADIO|LoFi Type#$plus1#0#8;Post Filter Type#=#OFF/LPF/HPF;Post Filter Cutoff#$hz#0#16;Radio Detune#0#127#;Radio Noise Level#0#127#;@BAL;@LV",
        "TELEPHONE|Voice Quality#0#15#;Treble#$g15#0#30;@BAL;@LV",
        "PHONOGRAPH|Signal Distortion#0#127#;Frequency Range#0#127#;Disc Type#=#LP/EP/SP;Scratch Noise Level#0#127#;Dust Noise Level#0#127#;Hiss Noise Level#0#127#;Total Noise Level#0#127#;Wow#0#127#;Flutter#0#127#;Random#0#127#;Total Wow/Flutter#0#127#;@BAL;@LV",
        "TAPE ECHO|Mode#=#S/M/L/S+M/S+L/M+L/S+M+L;Repeat Rate#0#127#;Intensity#0#127#;Bass#$g15#0#30;Treble#$g15#0#30;Head S Pan#$pan#0#127;Head M Pan#$pan#0#127;Head L Pan#$pan#0#127;Tape Distortion#0#5#;Wow/Flutter Rate#0#127#;Wow/Flutter Depth#0#127#;Echo Level#0#127#;Direct Level#0#127#;@LV",
        "FBK PITCH SHIFTER|Mode#$plus1#0#4;Coarse#$off24#0#36;Fine#$fine#0#100;Pre Delay#$pre#0#125;Feedback#-98#98#%;@PAN;@LG;@HG;@BAL;@LV",
        "2Vo PITCH SHIFTER|Mode#$plus1#0#4;Coarse A#$off24#0#36;Coarse B#$off24#0#36;Fine A#$fine#0#100;Fine B#$fine#0#100;Pre Delay A#$pre#0#125;Pre Delay B#$pre#0#125;Pan A#$pan#0#127;Pan B#$pan#0#127;Level Balance#$lbal#0#100;@BAL;@LV",
        "GATED REVERB|Type#=#NORMAL/REVERSE/SWEEP1/SWEEP2;@PRED;Time#~#raw;Pan#$pan#0#127;@LG;@HG;@BAL;@LV",
        "STEREO DELAY|Feedback Mode#=#NORMAL/CROSS;Delay Left#$delay#1#2022;Delay Right#$delay#1#2022;Phase Left#=#NORMAL/INVERT;Phase Right#=#NORMAL/INVERT;@FB;@HFD;@LG;@HG;@BAL;@LV",
        "MODULATION DELAY|Feedback Mode#=#NORMAL/CROSS;Delay Left#$delay#1#2022;Delay Right#$delay#1#2022;@FB;@HFD;@RATEH;@DEPTH;Phase#$x2#0#90;@LG;@HG;@BAL;@LV",
        "TRIPLE TAP DELAY|Delay Left#$delay4#1#4022;Delay Right#$delay4#1#4022;Delay Center#$delay4#1#4022;@FB;@HFD;Left Level#0#127#;Right Level#0#127#;Center Level#0#127#;@LG;@HG;@BAL;@LV",
        "QUADRUPLE TAP DELAY|@D4;Level 1#0#127#;Level 2#0#127#;Level 3#0#127#;Level 4#0#127#;@FB;@HFD;@BAL;@LV",
        "MULTI TAP DELAY|@D4;Pan 1#$pan#0#127;Pan 2#$pan#0#127;Pan 3#$pan#0#127;Pan 4#$pan#0#127;Level 1#0#127#;Level 2#0#127#;Level 3#0#127#;Level 4#0#127#;@FB;@HFD;@LG;@HG;@BAL;@LV",
        "REVERSE DELAY|Threshold#0#127#;Delay 1#$delay#1#2022;Delay 2#$delay#1#2022;Delay 3#$delay#1#2022;Delay 4#$delay#1#2022;Feedback 1#$fb#0#98;Feedback 4#$fb#0#98;HF Damp 1#$hzb#0#17;HF Damp 4#$hzb#0#17;Pan 1#$pan#0#127;Pan 2#$pan#0#127;Pan 3#$pan#0#127;Level 1#0#127#;Level 2#0#127#;Level 3#0#127#;@BAL;@LG;@HG;@LV",
        "SHUFFLE DELAY|Delay#$delay4#1#4022;Shuffle Rate#0#100#%;Pan A#$pan#0#127;Pan B#$pan#0#127;Level Balance#$lbal#0#100;@FB;Acceleration#0#15#;@HFD;@LG;@HG;@BAL;@LV",
        "TIME CONTROL DELAY|Delay#$delay4#1#4022;@FB;Acceleration#0#15#;@HFD;@PAN;@LG;@HG;@BAL;@LV",
        "TIME SKIP DELAY|Delay#$delay4#1#4022;Skip Rate#$r05#0#200;@FB;Acceleration#0#15#;@HFD;@PAN;@LG;@HG;@BAL;@LV",
    };

    const char* const reverbSpec[] =
    {
        "Reverb|Type#=#ROOM1/ROOM2/STAGE1/STAGE2/HALL1/HALL2/DELAY/PAN-DELAY;Time#0#127#;HF Damp#$hzb#0#17;Delay Feedback#0#127#;Level#0#127#",
        "SRV Room|Pre Delay#$pre#0#125;Time#0#127#;Size#$plus1#0#7;High Cut#$hzr#0#20;Density#0#127#;Diffusion#0#127#;LF Damp Freq#$hzlf#0#19;LF Damp Gain#$g36#0#36;HF Damp Freq#$hzhf#0#5;HF Damp Gain#$g36#0#36;Level#0#127#",
        "SRV Hall|Pre Delay#$pre#0#125;Time#0#127#;Size#$plus1#0#7;High Cut#$hzr#0#20;Density#0#127#;Diffusion#0#127#;LF Damp Freq#$hzlf#0#19;LF Damp Gain#$g36#0#36;HF Damp Freq#$hzhf#0#5;HF Damp Gain#$g36#0#36;Level#0#127#",
        "SRV Plate|Pre Delay#$pre#0#125;Time#0#127#;Size#$plus1#0#7;High Cut#$hzr#0#20;Density#0#127#;Diffusion#0#127#;LF Damp Freq#$hzlf#0#19;LF Damp Gain#$g36#0#36;HF Damp Freq#$hzhf#0#5;HF Damp Gain#$g36#0#36;Level#0#127#",
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
            else if (f[1].startsWith ("$"))
            {
                p.kind = Param::Kind::integer;
                p.scale = f[1].substring (1);
                p.lo = f[2].getIntValue();
                p.hi = f.size() > 3 ? f[3].getIntValue() : 0;
                p.unit = scaleUnit (p.scale);
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

namespace
{
    const char* const hzList[] = { "200","250","315","400","500","630","800","1000","1250","1600","2000","2500","3150","4000","5000","6300","8000","BYPASS" };
    const char* const hzrList[] = { "160","200","250","315","400","500","630","800","1000","1250","1600","2000","2500","3150","4000","5000","6300","8000","10000","12500","BYPASS" };
    const char* const hzlfList[] = { "50","63","80","100","125","160","200","250","315","400","500","630","800","1000","1250","1600","2000","2500","3150","4000" };
    const char* const hzhfList[] = { "4000","5000","6400","8000","10000","12500" };
    const char* const hzboList[] = { "50","56","63","71","80","90","100","112","125" };
    const char* const notes[] = { "1/64T","1/64","1/32T","1/32","1/16T","1/32.","1/16","1/8T","1/16.","1/8","1/4T","1/8.","1/4","1/2T","1/4.","1/2","1T","1/2.","1","2T","1.","2" };

    template <size_t N> juce::String pick (const char* const (&a)[N], int v, const char* unit = "")
    {
        const auto t = juce::String (a[juce::jlimit (0, (int) N - 1, v)]);
        return t == "BYPASS" ? t : t + (unit[0] ? juce::String (" ") + unit : juce::String());
    }
    juce::String signedStr (int n) { return (n > 0 ? "+" : "") + juce::String (n); }
}

juce::String scaleUnit (const juce::String& s)
{
    if (s.startsWith ("hz") || s == "r05" || s == "r10") return "Hz";
    if (s == "pre" || s == "delay" || s == "delay4") return "ms";
    if (s.startsWith ("g")) return "dB";
    if (s == "x2") return "deg";
    if (s == "fb") return "%";
    if (s == "fine") return "cent";
    if (s == "off24") return "semi";
    return {};
}

juce::String formatScale (const juce::String& s, int v)
{
    if (s == "hz")    return pick (hzList, juce::jmin (v, 16), "Hz");
    if (s == "hzb")   return pick (hzList, v, "Hz");
    if (s == "hzr")   return pick (hzrList, v, "Hz");
    if (s == "hzlf")  return pick (hzlfList, v, "Hz");
    if (s == "hzhf")  return pick (hzhfList, v, "Hz");
    if (s == "hzbo")  return pick (hzboList, v, "Hz");
    if (s == "r05")   return juce::String (v * 0.05, 2) + " Hz";
    if (s == "r10")   return juce::String (v * 0.1, 1) + " Hz";
    if (s == "x2")    return juce::String (v * 2) + " deg";
    if (s == "fb")    return juce::String (2 * (v - 49)) + " %";
    if (s == "g15")   return signedStr (v - 15) + " dB";
    if (s == "g60")   return signedStr (v - 60) + " dB";
    if (s == "g36")   return juce::String (v - 36) + " dB";
    if (s == "plus1") return juce::String (v + 1);
    if (s == "off24") return signedStr (v - 24) + " semi";
    if (s == "off20") return signedStr (v - 20);
    if (s == "off127") return juce::String (v - 127);
    if (s == "fine")  return signedStr (2 * (v - 50)) + " cent";
    if (s == "pan")   { const int n = v - 64; return n < 0 ? "L" + juce::String (-n) : n > 0 ? "R" + juce::String (n) : juce::String ("0"); }
    if (s == "bal")   return "D" + juce::String (100 - v) + ":" + juce::String (v) + "W";
    if (s == "lbal")  return "A" + juce::String (100 - v) + ":" + juce::String (v) + "B";
    if (s == "pre")
    {
        if (v <= 50)  return juce::String (v * 0.1, 1) + " ms";
        if (v <= 60)  return juce::String (5.0 + (v - 50) * 0.5, 1) + " ms";
        if (v <= 100) return juce::String (10 + (v - 60)) + " ms";
        return juce::String (50 + (v - 100) * 2) + " ms";
    }
    if (s == "delay4")
        return v <= 4000 ? juce::String (v) + " ms" : juce::String (notes[juce::jlimit (0, 21, v - 4001)]);
    if (s == "delay")
        return v <= 2000 ? juce::String (v) + " ms" : juce::String (notes[juce::jlimit (0, 21, v - 2001)]);
    return juce::String (v);
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
