# MC-909 Editor — Audio Unit

An Audio Unit / VST3 / standalone editor for the Roland MC-909 Sampling Groovebox.
It speaks the MC-909's published SysEx implementation directly, so the hardware
can be programmed from inside a DAW instead of from the front panel.

This is an original implementation written against Roland's *MC-909 MIDI
Implementation* document (model ID `00 59`). It is not a port, decompilation, or
derivative of Roland's own MC-909 Editor, and it contains none of that
application's code, resources, or interface artwork. The MIDI protocol itself is
a published interface specification; the code below is new work.

---

## Why this isn't a "conversion"

The uploaded `MC-909Editor.exe` is Roland's own editor, v1.22, © Roland
Corporation 2003–2006 — a 32-bit Windows MFC application from March 2006. Three
things follow from that:

1. **There is no conversion path.** An `.exe` is a Windows PE executable. An AU
   is a macOS bundle exporting a Component Manager / AUv2 entry point. Nothing
   translates between them; the only route is a rewrite, whatever the source.
2. **It was never a plugin.** The original is a standalone librarian/editor that
   talks to the hardware over MIDI. Its "engine" is entirely SysEx message
   construction — there is no audio DSP inside it to port.
3. **The valuable part is public.** Everything the original does is defined by
   Roland's parameter address map, which Roland published. Working from the
   specification produces a better result than reverse-engineering the binary
   would, and avoids copying protected expression.

## What the hardware exposes

| Block | Address | Contents |
|---|---|---|
| Setup | `01 00 00 00` | Effect switches, arpeggio, chord |
| System | `02 00 00 00` | Master tune/level, scale tune, mastering comp |
| Part Info | `10 00 00 00` | Voice reserve, MFX1/2, reverb, comp/EQ, 16 parts |
| Temporary Patch/Rhythm | `11 00 00 00` … `14 60 00 00` | Live edit buffer, one per part |
| Arpeggio / Chord | `15 00 00 00`, `18 00 00 00` | Pattern data |

Per part the edit buffer holds Patch Common (`0x51` bytes), the Tone Mix Table
(`0x29`), and four Tones of `0x10B` bytes each — filter, both envelopes, two
LFOs with waveform morphing, FXM, and the wave assignment.

## Implementation notes

**Base-128 address arithmetic.** Roland addresses are four *7-bit* bytes. Part 1
is `11 00 00 00` and Part 16 is `14 60 00 00`, which only works out if you carry
at 128, not 256. `roland::Address` does this; treating the address as a plain
32-bit integer is the single most common way a hand-written Roland editor ends
up writing to the wrong part.

**Nibbled parameters.** Entries marked `#` in the address map transmit as 2 or 4
bytes carrying one nibble each. `toNibbles`/`fromNibbles` handle Master Tune,
LFO rate, delay time, and the wave numbers.

**Direct CoreMIDI, not host MIDI.** `MidiHub` opens its own MIDI ports rather
than emitting SysEx from `processBlock`. Most DAWs filter, reorder, or drop
SysEx on a plugin's MIDI output; going straight to CoreMIDI makes the plugin
behave the same in Logic, Live, Bitwig, Reaper, and standalone.

**Two transports.** Individual edits go out as DT1 (`12`). Host automation goes
out as Quick SysEx (model ID `5D`) — four bytes shorter, and it addresses
several tones in one message, which is what the hardware's own knobs use. Bulk
dumps are paced at 20 ms per packet, as the device requires.

**Automation scope.** Twelve parameters are exposed to the host: cutoff,
resonance, filter envelope depth, filter and amp attack/release, LFO rate and
its three depths, and pan. Exposing all ~900 addressable parameters would swamp
the automation list and saturate a 31.25 kbaud MIDI link. Everything else is
editable in the UI.

## Getting a built plugin without a toolchain

`.github/workflows/build.yml` builds everything on GitHub's macOS runners. You
need no Xcode, no CMake, and no compiler on your own machine.

1. Create an empty repository on GitHub.
2. Upload this folder to it (drag and drop in the browser works — just keep the
   `.github` folder, GitHub's uploader includes it).
3. Open the **Actions** tab. The build starts on its own; if not, pick *Build*
   and press *Run workflow*.
4. Wait about five minutes. The finished run has an artifact called
   **MC-909-Editor-macOS** containing `AU.zip`, `VST3.zip` and `Standalone.zip`.
5. Unzip `AU.zip` and drop `MC-909 Editor.component` into
   `~/Library/Audio/Plug-Ins/Components/`.

The workflow ad-hoc signs the bundles, which is what stops Apple Silicon hosts
from silently rejecting them. It is not notarisation, so the standalone app will
still show a Gatekeeper prompt the first time; right-click → Open clears it.

Pushing a tag such as `v0.1.0` additionally publishes a permanent GitHub
release, which is handy if you want a stable download link.

If a step fails, the log in the Actions tab shows exactly which file and line —
that is the thing to fix, and the fix costs a commit, not a local setup.

### Building locally instead

Requires macOS, Xcode command line tools, and CMake 3.22+. JUCE is fetched
automatically.

```bash
cmake -B build -G Xcode
cmake --build build --config Release
auval -v aufx M909 Indp
```

`COPY_PLUGIN_AFTER_BUILD` installs to `~/Library/Audio/Plug-Ins/Components/`.

## Using it

1. Connect the MC-909 over USB or a MIDI interface.
2. On the hardware: SYSTEM → MIDI, set **Rx Exclusive** to ON and note the
   **Device ID** (default 17, which is `0x10` on the wire).
3. Load the plugin, pick the MIDI in and out ports, press **Detect** — the
   status line confirms an identity reply.
4. **Get from MC-909** pulls the current edit buffer for the selected part.
5. Edit. Every move sends a DT1 immediately.

The plugin state travels with the DAW session, so a project reopens with the
patch you were working on even though the hardware has moved on.

## Searching the factory sounds

**Search...** in the header (or **Ctrl/Cmd+F**) opens a browser over everything
printed in the patch list appendix: 693 waveforms, 800 preset patches (banks
A-G), the 72 preset rhythm sets, 215 preset patterns, and the pattern / RPS sets.
It filters as you type: every word has to match, anywhere in the name, location
or contents, so `909 kick` finds the TR909 kick waveforms *and* the kits that
contain one, and `132 q'hey` finds patterns by tempo and programmer.

| Key | Does |
| --- | --- |
| Up / Down, PageUp / PageDown | move through the results |
| Enter | load the selected item, close |
| Shift+Enter | load and keep browsing |
| Tab / Shift+Tab | next / previous category |
| Esc | clear the search; Esc again closes |

What Enter does is shown in the footer for the selected item:

- **Waveform** - sets Wave Group to INT, Wave L to the waveform number and Wave R
  to off, on the selected pad (Rhythm tab) or the selected tone (any other tab).
- **Patch / rhythm kit** - Bank Select + Program Change on the selected part's MIDI
  channel (preset patches: CC#0 = 81, CC#32 = 64-70; preset rhythm sets:
  CC#0 = 82, CC#32 = 64), then reads the unit back. Needs **Rx Bank Select** and
  **Rx Program Change** ON under SYSTEM -> MIDI, which is the factory default.
- **Patterns and sets** - reference only; the name is copied to the clipboard.

With **Audition** ticked, a test note is played after loading.

The line above the on-screen keyboard shows what the search last loaded on the
selected part ("Now loaded: Preset A 001 Trance Chord"). It appears immediately;
the **Patch** box at the top right shows the name the unit itself reports once the
read-back has arrived.

The on-screen keyboard fills the width of the editor, and the 16 keys the pads
play (Rhythm Tone Key #59-#74, B2 to D4 in the keyboard's labelling) carry the
pad numbers 1-16.

The data is a generated table (`Source/SearchData.cpp`) built from
`Roland_MC-909_Patch_List.pdf`. To regenerate it (needs `pdftotext`):

```
python3 tools/gen_search_data.py
```

The script checks the parsed counts against the printed lists and refuses to
write anything if they don't match.

## The Master tab

The MC-909's mastering effect is a three-band compressor. The Master tab draws it:

- **Band graph** - the three bands across the frequency range, each at its output Level.
  Drag a crossover line sideways to move the split (it snaps to the unit's own steps,
  200-800 Hz and 2-8 kHz); drag inside a band up or down to set that band's Level.
- **Compression curve** per band - input against output, with the gain taken off shaded in.
  Drag the dot on the curve to set the Threshold, and the open dot at the right-hand end
  to set the Ratio.
- **Dials** for Attack, Release, Threshold, Ratio and Level, written in real units
  (ms, dB, 1:2.5).
- **Live spectrum and meters** - a spectrum behind the band graph, a level meter beside
  each band's curve (RMS bar with a peak-hold tick), and an Output panel with left/right
  peaks and momentary / short-term loudness in LUFS (ITU-R BS.1770 K-weighting). The
  bands are split at the same crossover frequencies as the unit's.

The curves and dials are drawn from the settings. The spectrum and meters read the audio
that reaches the plugin, so they only move when the MC-909's audio return is routed through
the track the editor sits on (in Standalone, when an audio input is set up for it). The
plugin only looks at that audio; it passes it through unchanged. Because the return comes
from the unit's output, it is the sound AFTER the unit's own compressor: the signal before it
is not available, so gain reduction cannot be metered. The "GR at 0 dB in" figure on each
curve is the setting's maximum reduction, not a measurement.

The Release readout is approximate (shown with a "~"): the unit documents 0-100 as
50-5000 ms but not the curve in between, so a logarithmic one is assumed.

## The Comp/EQ tab

- **Compressor** - an input/output curve. Drag the dot on the curve for Threshold and the
  open dot at the right-hand end for Ratio. The curve is a *schematic*: Roland publishes the
  ranges (Threshold 0-127, Ratio 0-19) but not what each step is worth in dB or in ratio, so
  it shows how the two settings interact and the axes carry no numbers. Attack and Release
  are shown in the documented ms ranges; the On/Off pill is the unit's Comp switch.
- **2-band EQ** - frequency response of the low and high shelves, using only documented
  values (low corner 200/400 Hz, high corner 2/4/8 kHz, +/-15 dB). Drag a dot sideways to pick
  the corner (it snaps to the unit's choices) and up/down to set the gain.
- **Output** - Level, Reverb Send and Output (Dry / MFX1 / MFX2).

The compressor and EQ are one processor shared by the whole unit. A part uses it when its
Output Select (Part tab) is set to COMP.

## Known limits

- Sample data (`.wav` transfer over SMF/SmartMedia) is not covered by the SysEx
  implementation and isn't handled here.
- The sequencer's pattern data is not exposed over SysEx by the MC-909; only
  Quick SysEx part mute is.
- MFX parameters are addressable but their meaning changes per effect type
  (39 types for MFX1, 48 for MFX2), so those tabs are not yet built out.
- Rhythm set editing shares the tone structure but uses key-numbered blocks at
  `00 5C 00` upward; the address constants are present, the UI tab is not.

## Layout

```
Source/
  RolandSysEx.{h,cpp}   Address arithmetic, checksum, DT1/RQ1/Quick, parsing
  MC909Map.{h,cpp}      Block addresses and the parameter table
  MidiHub.{h,cpp}       CoreMIDI ports + paced send queue
  PluginProcessor.*     Edit buffer mirror, automation, session state
  PluginEditor.*        UI generated from the parameter table
  SearchOverlay.*       The Ctrl/Cmd+F browser
  SearchIndex.*         Search ranking (no JUCE dependency)
  SearchData.cpp        Generated: every waveform / patch / kit / pattern name
tools/
  gen_search_data.py    Regenerates SearchData.cpp from the patch list PDF
```

Adding a parameter means adding one line to `MC909Map.cpp`; the UI, the SysEx
encoding, and state saving all follow from the table.
