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
```

Adding a parameter means adding one line to `MC909Map.cpp`; the UI, the SysEx
encoding, and state saving all follow from the table.
