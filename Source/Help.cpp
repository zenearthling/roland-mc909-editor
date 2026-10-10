#include "Help.h"
#include <map>

namespace help
{
namespace
{
    using Table = std::map<juce::String, juce::String>;

    const Table& table()
    {
        static const Table t =
        {
            // ---- tone / pad common -------------------------------------------------
            { "level",      "Output level of this tone before it is mixed with the others in the patch." },
            { "pan",        "Stereo position of this tone. Negative is left, 0 is centre, positive is right." },
            { "pan_kf",     "How much the pan position follows the note you play. Positive values spread higher notes to the right." },
            { "rnd_pan",    "Random pan: how far the position can wander left or right each time a note is played. 0 keeps it fixed." },
            { "alt_pan",    "Alternate pan: successive notes swing alternately left and right by this amount." },
            { "rx_pan_mode","How this tone responds to incoming pan messages. See the Owner's Manual for the modes." },
            { "coarse",     "Pitch in semitone steps, up to four octaves either way." },
            { "fine",       "Pitch in cents (hundredths of a semitone), half a semitone either way." },
            { "rnd_pitch",  "Random pitch: the range, in cents, by which the pitch changes randomly on every note. 0 turns it off." },
            { "pitch_kf",   "Pitch keyfollow. +100 is a normal keyboard (one octave per 12 notes), 0 plays every note at one pitch, negative values invert it." },
            { "wave_group", "Which bank the waveform comes from: internal memory, an SRX expansion board, preset samples, or user/card samples." },
            { "wave_gid",   "Waveform group ID. Part of the wave address; leave it unless you know the wave you want." },
            { "wave_l",     "Waveform number for the left (or mono) channel. 0 is off. Drag or double-click to type a number." },
            { "wave_r",     "Waveform number for the right channel. Set 0 (off) to use the left wave on both sides." },
            { "wave_gain",  "Waveform gain in 6 dB steps (+6 dB doubles the level). Use the top setting before distortion from the booster." },
            { "fxm_sw",     "Frequency Cross Modulation: modulates the wave's frequency with another wave for rough, metallic, bell-like sounds." },
            { "fxm_color",  "FXM colour, 1 to 4. Higher is rougher, lower is more metallic." },
            { "fxm_depth",  "How strongly FXM modulates the wave, 0 to 16." },
            { "tempo_sync", "Locks LFO and delay times to the unit's tempo instead of free-running rates." },
            { "env_mode",   "Envelope mode: NO-SUS ends the sound by itself after the envelope (typical for drums), SUSTAIN holds until the pad is released." },

            // ---- pitch envelope -----------------------------------------------------
            { "penv_depth", "Depth of the pitch envelope. Larger values bend the pitch further; negative values invert the shape." },
            { "penv_t",     "Pitch envelope times T1 to T4. Higher values take longer to reach the next level. The envelope display shows the shape." },
            { "penv_l",     "Pitch envelope levels L0 to L4: how far above or below the base pitch each point sits." },
            { "penv_vsens", "How much playing strength changes the pitch envelope depth. Negative values reverse the effect." },
            { "penv_t1_vs", "How much playing strength speeds up (+) or slows down (-) the first pitch envelope time." },
            { "penv_t4_vs", "How much release speed changes the last pitch envelope time." },
            { "penv_t_kf",  "How the pitch envelope times shorten (+) or lengthen (-) for notes above middle C." },

            // ---- filter -------------------------------------------------------------
            { "tvf_type",   "Filter type. LPF removes highs, HPF removes lows, BPF keeps a band, PKG boosts a band. LPF2 and LPF3 are gentler low-passes that ignore resonance." },
            { "tvf_cutoff", "Cutoff frequency: where the filter starts working. On a low-pass, lowering it darkens the sound and raising it brightens." },
            { "tvf_cut_kf", "Cutoff keyfollow: how the cutoff rises (+) or falls (-) for notes above middle C." },
            { "tvf_cut_vcrv","Velocity curve (FIX or 1 to 7) for how playing strength affects the cutoff. FIX ignores strength." },
            { "tvf_cut_vsens","How strongly playing dynamics move the cutoff. Negative values invert the effect." },
            { "tvf_reso",   "Resonance: emphasis around the cutoff. More gives a sharper, more vocal character; too much will whistle or distort." },
            { "tvf_reso_vs","How strongly playing dynamics change the resonance." },
            { "tvf_env_vcrv","Velocity curve for how playing strength affects the filter envelope depth." },
            { "tvf_env_vsens","How strongly playing dynamics change the filter envelope depth." },
            { "tvf_env_depth","Depth of the filter envelope. Negative values invert it, so the cutoff sweeps down instead of up." },
            { "tvf_t",      "Filter envelope times T1 to T4. Higher values take longer to reach the next cutoff level." },
            { "tvf_l",      "Filter envelope levels L0 to L4: the cutoff offset at each point of the envelope." },
            { "tvf_t1_vs",  "How playing strength speeds up (+) or slows down (-) the first filter envelope time." },
            { "tvf_t4_vs",  "How release speed changes the last filter envelope time." },
            { "tvf_t_kf",   "How the filter envelope times shorten (+) or lengthen (-) for notes above middle C." },

            // ---- amp ----------------------------------------------------------------
            { "tva_vcrv",   "Velocity curve for how playing strength affects the volume." },
            { "tva_vsens",  "How strongly playing dynamics change the volume. Negative values make harder hits quieter." },
            { "tva_t",      "Amp envelope times T1 to T4: attack, decay, and release shape of the volume. Higher is slower." },
            { "tva_l",      "Amp envelope levels L1 to L3. The start and end are fixed at silence." },
            { "tva_t1_vs",  "How playing strength speeds up (+) or slows down (-) the attack time." },
            { "tva_t4_vs",  "How release speed changes the release time." },
            { "tva_t_kf",   "How the amp envelope times shorten (+) or lengthen (-) for notes above middle C." },
            { "bias_dir",   "Which notes the level bias affects: LWR (below the bias position), UPR (above), L&U (both sides) or ALL." },
            { "bias_pos",   "The note number around which the volume changes." },
            { "bias_level", "How much the volume changes with distance from the bias position. Use it to fade a tone out across the keyboard." },

            // ---- sends / receive ----------------------------------------------------
            { "dry_send",   "Amount of the tone sent directly to the output, bypassing the effects." },
            { "rev_send",   "Amount of the tone sent to the reverb." },
            { "rev_send_mfx","Amount of the tone's effect-processed signal sent to the reverb." },
            { "rx_bender",  "Whether this tone responds to pitch bend." },
            { "rx_expr",    "Whether this tone responds to expression (CC11)." },
            { "rx_hold",    "Whether this tone responds to hold pedal messages (CC64)." },
            { "redamper",   "Re-damper (half-pedal) response for this tone." },
            { "ctrl",       "Controller routing for this tone. The Owner's Manual describes the available settings." },
            { "lfo",        "LFO setting for this tone." },
            { "delay_mode", "Delay mode for the tone start delay. See the Owner's Manual." },
            { "delay_time", "Start delay for the tone after a note is played." },

            // ---- LFO ----------------------------------------------------------------
            { "lfo.rate",     "Speed of the LFO. With tempo sync on, this is a note value instead of a free rate." },
            { "lfo.offset",   "Shifts the LFO waveform up or down, so the modulation swings mostly one way." },
            { "lfo.detune",   "Slightly randomises the LFO rate per note, so stacked notes drift apart." },
            { "lfo.delay",    "Time from pressing (or releasing) a pad until the LFO begins to take effect." },
            { "lfo.delay_kf", "Changes the LFO delay by note relative to C4. Positive values apply the LFO sooner on higher notes." },
            { "lfo.fade_mode","How the LFO fades in or out: on or off, in or out, after the delay." },
            { "lfo.fade_time","Time the LFO takes to fade in or out." },
            { "lfo.key_trig", "On restarts the LFO from the beginning of its cycle on every note." },
            { "lfo.pitch_dep","How strongly the LFO modulates pitch (vibrato). The centre value is no effect." },
            { "lfo.tvf_dep",  "How strongly the LFO modulates the filter cutoff (wah). The centre value is no effect." },
            { "lfo.tva_dep",  "How strongly the LFO modulates volume (tremolo). The centre value is no effect." },
            { "lfo.pan_dep",  "How strongly the LFO modulates pan (auto-pan). The centre value is no effect." },
            { "lfo.morph",    "Morphs the LFO shape smoothly through the waveforms printed on the panel." },

            // ---- patch common -------------------------------------------------------
            { "patch.level",  "Overall patch level." },
            { "patch.pan",    "Overall patch pan." },
            { "patch.coarse", "Pitch of the whole patch in semitones, on top of each tone's own tuning." },
            { "patch.fine",   "Pitch of the whole patch in cents." },
            { "patch.octave", "Shifts the whole patch up or down in octaves." },
            { "patch.analog_feel", "Adds the slight pitch drift of an analogue synth." },
            { "patch.stretch","Stretch tune: widens or narrows the tuning of the upper and lower keyboard, like a piano." },
            { "patch.mono_poly","MONO plays one note at a time, POLY lets notes stack." },
            { "patch.priority","Note priority for mono playing: which note takes over when several are held." },
            { "patch.legato", "In mono mode, overlapping notes glide on without retriggering the envelope." },
            { "patch.legato_retrig","Retriggers the envelope on legato notes." },
            { "patch.bend_up","Pitch bend range upward, in semitones." },
            { "patch.bend_down","Pitch bend range downward, in semitones." },
            { "patch.porta_sw","Portamento: pitch glides from one note to the next. Turning it on also sets the patch to mono." },
            { "patch.porta_mode","Normal glides always; Legato glides only between overlapping notes." },
            { "patch.porta_type","Whether the glide takes a constant time or a constant rate." },
            { "patch.porta_start","Where the glide starts from: the previous pitch, or from the pitch you are already at." },
            { "patch.porta_time","How long the glide takes. Larger is slower." },
            { "patch.unison_sw","Unison: switches the detune layering effect on or off." },
            { "patch.unison_fat","Unison Fat: between far left and centre it detunes the layers; between centre and far right it adds harmony." },
            { "patch.cutoff_ofs","Offsets the cutoff of every tone in the patch. The panel knob does the same." },
            { "patch.reso_ofs","Offsets the resonance of every tone in the patch." },
            { "patch.attack_ofs","Offsets the envelope attack of every tone in the patch." },
            { "patch.release_ofs","Offsets the envelope release of every tone in the patch." },
            { "patch.velsens_ofs","Offsets how strongly playing strength affects every tone." },
            { "patch.tmt_ctrl","Tone mix table control setting for the patch. See the Owner's Manual." },
            { "patch.category","Category label stored with the patch." },

            // ---- tone mix table -----------------------------------------------------
            { "tmt.vel_control","Velocity control: OFF plays all enabled tones, ON switches tones by playing strength, RANDOM picks tones at random." },
            { "tmt.structure12","How tones 1 and 2 are combined. Types 2 to 10 are booster and ring-modulating combinations (see the Owner's Manual)." },
            { "tmt.structure34","How tones 3 and 4 are combined." },
            { "tmt.booster12","Booster gain for tones 1 and 2 when a booster structure is used. It adds distortion." },
            { "tmt.booster34","Booster gain for tones 3 and 4 when a booster structure is used." },

            // ---- rhythm pad only ----------------------------------------------------
            { "rn.assign",   "Single plays several hits of this pad together; Multi lets only one sound at a time." },
            { "rn.mute",     "Mute group: pads in the same group cut each other off, like open and closed hi-hats." },
            { "rn.rhy_level","Level of the whole pad (all four tones)." },
            { "rn.bend_range","Pitch bend range for this pad, in semitones." },
            { "t.sw",        "Switches this tone on or off for the pad." },
            { "t.vel_lo",    "Lowest playing strength at which this tone sounds." },
            { "t.vel_hi",    "Highest playing strength at which this tone sounds." },
            { "t.fade_lo",   "How gradually the tone fades in below its lowest velocity. 0 is a hard edge." },
            { "t.fade_hi",   "How gradually the tone fades out above its highest velocity. 0 is a hard edge." },

            // ---- effects ------------------------------------------------------------
            { "mfx1.type",   "Effect type for MFX 1. The parameters below change meaning with the type." },
            { "mfx2.type",   "Effect type for MFX 2. The parameters below change meaning with the type." },
            { "rev.type",    "Reverb type. The parameters below change meaning with the type." },
            { "mfx1.rev_send","How much of MFX 1's output goes to the reverb." },
            { "mfx2.rev_send","How much of MFX 2's output goes to the reverb." },
            { "mfx1.out",    "Where MFX 1's output goes." },
            { "comp_sw",     "Compressor on or off for the unit." },
            { "mfx1_sw",     "MFX 1 on or off for the unit." },
            { "mfx2_sw",     "MFX 2 on or off for the unit." },
            { "rev_sw",      "Reverb on or off for the unit." },

            // ---- part / system ------------------------------------------------------
            { "part.rx_switch","Whether this part responds to incoming MIDI." },
            { "part.bank_msb","Bank Select MSB for this part." },
            { "part.bank_lsb","Bank Select LSB for this part." },
            { "part.program","Program number for this part." },
            { "part.level",  "Level of this part in the mixer." },
            { "part.pan",    "Pan of this part in the mixer." },
            { "part.coarse", "Transposes this part in semitones." },
            { "part.fine",   "Fine tunes this part in cents." },
            { "part.output", "Which output pair this part goes to." },
            { "part.dry_send","Amount of this part sent dry to its output." },
            { "part.rev_send","Amount of this part sent to the reverb." },
            { "part.auto_sync","Auto Sync for this part: synchronises a sample to the pattern tempo." },
            { "sys.master_level","Master level of the whole unit." },
            { "sys.master_tune","Master tune of the whole unit in cents." },
            { "sys.key_shift","Transposes the whole unit in semitones." },
            { "sys.scale_tune","Scale tuning of the whole unit." },
            { "sys.patch_remain","Whether notes that are sounding keep playing when you select another patch or rhythm set." },
            { "sys.rx_bank", "Whether bank select messages are received." },
            { "sys.rx_pc",   "Whether program change messages are received." },
        };
        return t;
    }

    juce::String lookup (const mc909::ParamDef& d)
    {
        const auto& t = table();
        const juce::String id = d.id;

        // exact id first (setup switches, rn.* extras, etc.)
        if (auto it = t.find (id); it != t.end())
            return it->second;

        // strip the family prefix: "tone.xxx", "rn.xxx", "rn.t.xxx"
        juce::String s = id;
        if (s.startsWith ("rn.t."))       s = s.fromFirstOccurrenceOf ("rn.", false, false); // "t.xxx"
        else if (s.startsWith ("tone.lfo"))
            s = "lfo." + s.fromLastOccurrenceOf (".", false, false);
        else if (s.contains ("."))
            s = s.fromFirstOccurrenceOf (".", false, false);

        if (auto it = t.find (s); it != t.end())
            return it->second;

        // "rn.t.coarse" etc. share the tone texts
        if (s.startsWith ("t."))
            if (auto it = t.find (s.substring (2)); it != t.end())
                return it->second;

        return {};
    }
}

//==============================================================================
juce::String rangeLine (const mc909::ParamDef& d)
{
    if (d.isChoice())
    {
        juce::StringArray c = d.choices;
        if (c.size() > 12)
            return "Choose from " + juce::String (c.size()) + " options.";
        return "Options: " + c.joinIntoString (", ");
    }

    const int lo = d.toDisplay (d.rawMin), hi = d.toDisplay (d.rawMax);
    return "Range: " + juce::String (lo) + " to " + juce::String (hi);
}

juce::String describe (const mc909::ParamDef& d)
{
    if (auto s = lookup (d); s.isNotEmpty())
        return s;

    const juce::String id = d.id;

    if (id.startsWith ("mfx1.p") || id.startsWith ("mfx2.p") || id.startsWith ("rev.p"))
        return "An effect parameter. What it controls depends on the effect type chosen above, and the editor shows only its raw value. "
               "The Owner's Manual lists the parameters for each effect type.";

    if (id.contains ("mtx"))
        return "Matrix Control: routes the MATRIX CTRL knob (or the chosen source) to up to four destinations. "
               "Each destination has a sensitivity that sets how far and in which direction it moves.";

    if (id.startsWith ("comp."))
        return "Compressor and EQ setting for the part. See the Owner's Manual compressor section.";

    if (id.startsWith ("auto."))
        return "Host-automatable shortcut. Moves the matching parameter on the unit through Quick SysEx.";

    return "No description written for this parameter yet.";
}

} // namespace help
