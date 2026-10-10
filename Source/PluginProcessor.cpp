#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mc909;

//==============================================================================
namespace
{
    /** Host parameter whose displayed name follows what the knob is currently assigned to. */
    class MacroParam : public juce::AudioParameterFloat
    {
    public:
        explicit MacroParam (int n)
            : juce::AudioParameterFloat (juce::ParameterID { "macro." + juce::String (n), 1 },
                                         "Knob " + juce::String (n),
                                         juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
              number (n)
        {}

        void setLabel (const juce::String& l)
        {
            const juce::ScopedLock sl (lock);
            label = l;
        }

        juce::String getName (int maxLen) const override
        {
            const juce::ScopedLock sl (lock);
            const auto name = label.isEmpty() ? "Knob " + juce::String (number)
                                              : "Knob " + juce::String (number) + " " + label;
            return name.substring (0, maxLen);
        }

    private:
        int number;
        juce::CriticalSection lock;
        juce::String label;
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout MC909EditorProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Only the Quick SysEx set is exposed for host automation. Exposing all
    // ~900 addressable parameters would swamp the host's automation list and
    // saturate the MIDI link; these twelve are the ones worth a fader.
    for (const auto& ap : quick::automatable())
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { ap.id, 1 }, ap.name, 0, 127, 64));

    for (int n = 1; n <= numMacros; ++n)
        layout.add (std::make_unique<MacroParam> (n));

    return layout;
}

MC909EditorProcessor::MC909EditorProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      valueTree (*this, nullptr, "MC909", makeLayout())
{
    hub.addListener (this);
    hub.setSendInterval (5);

    for (const auto& ap : quick::automatable())
        valueTree.addParameterListener (ap.id, this);

    for (int n = 0; n < numMacros; ++n)
    {
        valueTree.addParameterListener ("macro." + juce::String (n + 1), this);
        macroLastRaw[n] = -1;
    }
    macroQuietUntil = juce::Time::getMillisecondCounter() + 2000;
    startTimer (10);

    // Allocate the local mirror so the UI has something coherent before the
    // first dump arrives.
    const Block all[] { Block::patchCommon, Block::patchTMT, Block::patchTone,
                        Block::partInfoPart, Block::compEQ,
                        Block::systemCommon, Block::mastering };
    for (auto b : all)
        blockData (b).assign (mc909::blockByteCount (b), 0);

   #ifdef MC909_UI_DEMO
    // Local test aid, never compiled into release builds: replay the RX lines of a saved
    // MIDI log through the normal receive path, so decoding can be checked without hardware.
    if (const auto path = juce::SystemStats::getEnvironmentVariable ("MC909_REPLAY_LOG", {}); path.isNotEmpty())
    {
        selectedPart = (int) juce::SystemStats::getEnvironmentVariable ("MC909_REPLAY_PART", "9").getIntValue();

        for (const auto& line : juce::StringArray::fromLines (juce::File (path).loadFileAsString()))
        {
            const auto tokens = juce::StringArray::fromTokens (line, " ", "");
            const int rx = tokens.indexOf ("RX");
            if (rx < 0) continue;

            std::vector<uint8_t> bytes;
            for (int i = rx + 1; i < tokens.size(); ++i)
            {
                const auto& t = tokens[i];
                if (t.length() != 2 || ! t.containsOnly ("0123456789abcdef")) break;
                bytes.push_back ((uint8_t) t.getHexValue32());
            }

            if (bytes.size() > 2 && bytes.front() == 0xF0 && bytes.back() == 0xF7)
                sysExReceived (juce::MidiMessage::createSysExMessage (bytes.data() + 1, (int) bytes.size() - 2));
        }
    }
   #endif
}

MC909EditorProcessor::~MC909EditorProcessor()
{
    stopTimer();

    for (const auto& ap : quick::automatable())
        valueTree.removeParameterListener (ap.id, this);

    for (int n = 0; n < numMacros; ++n)
        valueTree.removeParameterListener ("macro." + juce::String (n + 1), this);

    hub.removeListener (this);
}

//==============================================================================
void MC909EditorProcessor::processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer& midiMessages)
{
    // Audio passes through untouched; this plugin is a control surface, not a
    // processor. Incoming MIDI is forwarded so the plugin can sit inline on a
    // track without breaking the chain.
    juce::ignoreUnused (midiMessages);
}

juce::AudioProcessorEditor* MC909EditorProcessor::createEditor()
{
    return new MC909EditorComponent (*this);
}

//==============================================================================
juce::String MC909EditorProcessor::blockKey (Block b) const
{
    switch (b)
    {
        case Block::patchCommon:  return "patchCommon:" + juce::String (selectedPart);
        case Block::patchTMT:     return "patchTMT:"    + juce::String (selectedPart);
        case Block::patchTone:    return "patchTone:"   + juce::String (selectedPart)
                                                        + ":" + juce::String (selectedTone);
        case Block::partInfoPart: return "partInfo:"    + juce::String (selectedPart);
        case Block::compEQ:       return "compEQ";
        case Block::systemCommon: return "systemCommon";
        case Block::mastering:    return "mastering";
        case Block::rhythmCommon: return "rhythmCommon:" + juce::String (selectedPart);
        case Block::rhythmNote:   return blockKeyFor (b, selectedPad);
    }
    return "unknown";
}

juce::String MC909EditorProcessor::blockKeyFor (Block b, int pad) const
{
    jassert (b == Block::rhythmNote);
    juce::ignoreUnused (b);
    return "rhythmNote:" + juce::String (selectedPart) + ":" + juce::String (pad);
}

uint32_t MC909EditorProcessor::effectiveOffset (const ParamDef& p) const
{
    return p.offset + (p.toneStride != 0 ? (uint32_t) selectedTone * p.toneStride : 0u);
}

std::vector<uint8_t>& MC909EditorProcessor::blockData (Block b)
{
    auto& v = blocks[blockKey (b)];
    if (v.size() < mc909::blockByteCount (b))
        v.resize (mc909::blockByteCount (b), 0);
    return v;
}

const std::vector<uint8_t>* MC909EditorProcessor::findBlock (Block b) const
{
    const auto it = blocks.find (blockKey (b));
    return it == blocks.end() ? nullptr : &it->second;
}

int MC909EditorProcessor::readValue (const std::vector<uint8_t>& data, uint32_t offset, int numBytes)
{
    if (offset + (uint32_t) numBytes > data.size())
        return 0;

    return roland::fromNibbles (data.data() + offset, numBytes);
}

int MC909EditorProcessor::getParameterValue (const ParamDef& p) const
{
    if (const auto* data = findBlock (p.block))
        return readValue (*data, effectiveOffset (p), p.numBytes);

    return p.rawMin;
}

void MC909EditorProcessor::setParameterValue (const ParamDef& p, int rawValue, bool sendToDevice)
{
    rawValue = juce::jlimit (p.rawMin, p.rawMax, rawValue);

    std::vector<uint8_t> bytes;
    roland::toNibbles (rawValue, p.numBytes, bytes);

    const uint32_t offset = effectiveOffset (p);

    auto& data = blockData (p.block);
    for (size_t i = 0; i < bytes.size() && offset + i < data.size(); ++i)
        data[offset + i] = bytes[i];

    if (sendToDevice)
    {
        const auto addr = blockAddress (p.block, selectedPart, selectedTone, selectedPad) + offset;
        hub.send (roland::makeDT1 (deviceId, addr, bytes.data(), bytes.size()));
    }

    editListeners.call ([] (EditListener& l) { l.modelChanged(); });
}

//==============================================================================
juce::String MC909EditorProcessor::getPatchName() const
{
    const auto* data = findBlock (Block::patchCommon);
    if (data == nullptr || data->size() < 12)
        return {};

    juce::String s;
    for (int i = 0; i < 12; ++i)
    {
        const auto c = (*data)[(size_t) i];
        s += juce::juce_wchar (c >= 32 && c <= 127 ? c : ' ');
    }
    return s.trimEnd();
}

void MC909EditorProcessor::setPatchName (const juce::String& newName)
{
    auto padded = newName.substring (0, 12);
    while (padded.length() < 12)
        padded += " ";

    uint8_t bytes[12];
    for (int i = 0; i < 12; ++i)
    {
        const auto c = (int) padded[i];
        bytes[i] = (uint8_t) juce::jlimit (32, 127, c);
    }

    auto& data = blockData (Block::patchCommon);
    for (int i = 0; i < 12; ++i)
        data[(size_t) i] = bytes[i];

    const auto addr = blockAddress (Block::patchCommon, selectedPart, selectedTone);
    hub.send (roland::makeDT1 (deviceId, addr, bytes, 12));

    editListeners.call ([] (EditListener& l) { l.modelChanged(); });
}

//==============================================================================
void MC909EditorProcessor::requestBlock (Block b, int gapMs)
{
    const auto addr = blockAddress (b, selectedPart, selectedTone, selectedPad);
    hub.send (roland::makeRQ1 (deviceId, addr, mc909::blockByteCount (b)), gapMs);
}

void MC909EditorProcessor::requestAll()
{
    const Block order[] { Block::systemCommon, Block::mastering, Block::compEQ,
                          Block::partInfoPart, Block::patchCommon, Block::patchTMT };

    // Give the unit time to answer each request before the next one arrives.
    constexpr int requestGapMs = 150;

    dt1Stored = dt1Unplaced = sysexUnreadable = 0;
    hub.logLine ("--- Get from MC-909: part " + juce::String (selectedPart + 1) + " ---");

    for (auto b : order)
        requestBlock (b, requestGapMs);

    // All four tones of the selected part.
    const int savedTone = selectedTone;
    for (int t = 0; t < 4; ++t)
    {
        selectedTone = t;
        requestBlock (Block::patchTone, requestGapMs);
    }
    selectedTone = savedTone;

    // Rhythm kit (answered only when the part holds a rhythm set).
    requestRhythm (100);
}

void MC909EditorProcessor::requestRhythm (int gapMs)
{
    hub.send (roland::makeRQ1 (deviceId, blockAddress (Block::rhythmCommon, selectedPart, 0), mc909::kSizeRhythmCommon), gapMs);

    for (int pad = 0; pad < (int) mc909::kRhythmPads; ++pad)
    {
        const auto base = blockAddress (Block::rhythmNote, selectedPart, 0, pad);
        hub.send (roland::makeRQ1 (deviceId, base, mc909::kSizeRhythmNote), gapMs);

        // The unit leaves out any 4-byte value that straddles the edge of a reply, and
        // tone 4's wave number sits right on the 128-byte boundary. Ask for it alone.
        hub.send (roland::makeRQ1 (deviceId, base + (mc909::kRhythmToneBase + 3u * mc909::kRhythmToneSpan + 6u), 4), gapMs);
    }
}

void MC909EditorProcessor::probePatchSizes()
{
    // Kit dump v4: (1) pad 2 (note block at 5E 00) read in 16-byte chunks to find the
    // exact data boundaries, (2) the following 3rd-byte ranges (13 31 xx, 13 32 xx)
    // in 128-byte pages to find the remaining notes.
    const int gapMs = 100;
    hub.setFullHexLogging (true);
    hub.logLine ("--- KIT DUMP v4, part " + juce::String (selectedPart + 1) + " ---");

    const uint32_t base = (mc909::temporaryPart (selectedPart) + mc909::kTempRhythm).toLinear();

    const uint32_t pad2 = base + 0x5Eu * 128u;
    for (uint32_t i = 0; i < 16; ++i)
        hub.send (roland::makeRQ1 (deviceId, roland::Address::fromLinear (pad2 + i * 16u), 0x10), gapMs);

    for (uint32_t page = 0; page < 192; ++page)
        hub.send (roland::makeRQ1 (deviceId, roland::Address::fromLinear (base + 128u * 128u + page * 128u), 0x80), gapMs);
}

void MC909EditorProcessor::sendAll()
{
    std::vector<juce::MidiMessage> out;

    auto pushBlock = [&] (Block b)
    {
        // Never push a block we have not read: that would overwrite the unit with zeros.
        if (findBlock (b) == nullptr)
            return;

        const auto& data = blockData (b);
        const auto base  = blockAddress (b, selectedPart, selectedTone, selectedPad);

        // The device wants packets of 256 bytes or fewer; 128 keeps us clear of
        // the base-128 address boundary too, which makes the maths trivial.
        // A rhythm pad has a 4-byte value across the 128-byte edge, so its first packet is
        // 130 bytes and keeps that value whole.
        constexpr size_t chunk = 128;
        for (size_t pos = 0; pos < data.size();)
        {
            const size_t want = (b == Block::rhythmNote && pos == 0) ? 130 : chunk;
            const size_t n = juce::jmin (want, data.size() - pos);
            out.push_back (roland::makeDT1 (deviceId, base + (uint32_t) pos, data.data() + pos, n));
            pos += n;
        }
    };

    pushBlock (Block::partInfoPart);
    pushBlock (Block::patchCommon);
    pushBlock (Block::patchTMT);

    const int savedTone = selectedTone;
    for (int t = 0; t < 4; ++t)
    {
        selectedTone = t;
        pushBlock (Block::patchTone);
    }
    selectedTone = savedTone;

    // Rhythm pads are deliberately not part of this bulk send: every edit already goes
    // out live, and a pad block that was only partly read must never be written back.

    // Bulk transfers need breathing room, so slow the queue down for the burst.
    hub.setSendInterval (20);
    hub.sendAll (out);
}

//==============================================================================
void MC909EditorProcessor::setSelectedPart (int part)
{
    selectedPart = juce::jlimit (0, 15, part);
    editListeners.call ([] (EditListener& l) { l.modelChanged(); });
}

void MC909EditorProcessor::setSelectedTone (int tone)
{
    selectedTone = juce::jlimit (0, 3, tone);
    editListeners.call ([] (EditListener& l) { l.modelChanged(); });
}

const ParamDef* MC909EditorProcessor::getMacroDef (int slot) const
{
    return juce::isPositiveAndBelow (slot, numMacros) ? macroDef[slot] : nullptr;
}

juce::RangedAudioParameter* MC909EditorProcessor::getMacroParameter (int slot)
{
    return valueTree.getParameter ("macro." + juce::String (slot + 1));
}

void MC909EditorProcessor::assignMacro (int slot, const ParamDef* def)
{
    if (! juce::isPositiveAndBelow (slot, numMacros))
        return;

    macroDef[slot] = def;
    macroLastRaw[slot] = -1;

    if (auto* mp = dynamic_cast<juce::AudioParameterFloat*> (getMacroParameter (slot)))
    {
        // Put the knob where the parameter currently is, without sending anything.
        if (def != nullptr && def->rawMax > def->rawMin)
        {
            const float v = (float) (getParameterValue (*def) - def->rawMin) / (float) (def->rawMax - def->rawMin);
            macroSuppress = true;
            mp->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
            macroSuppress = false;
        }

        // Rename it in the host (see MacroParam::getName).
        if (auto* m = dynamic_cast<MacroParam*> (mp))
            m->setLabel (def != nullptr ? def->name : juce::String());
    }

    updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));
    editListeners.call ([] (EditListener& l) { l.modelChanged(); });
}

void MC909EditorProcessor::timerCallback()
{
    const uint32_t mask = macroDirty.exchange (0);
    if (mask == 0)
        return;

    for (int n = 0; n < numMacros; ++n)
    {
        if ((mask & (1u << n)) == 0 || macroDef[n] == nullptr)
            continue;

        const auto& d = *macroDef[n];
        const int raw = juce::jlimit (d.rawMin, d.rawMax,
                                      d.rawMin + juce::roundToInt (macroValue[n].load() * (float) (d.rawMax - d.rawMin)));

        if (raw != macroLastRaw[n])
        {
            macroLastRaw[n] = raw;
            setParameterValue (d, raw, true);
        }
    }
}

void MC909EditorProcessor::setSelectedPad (int pad)
{
    selectedPad = juce::jlimit (0, (int) mc909::kRhythmPads - 1, pad);
    editListeners.call ([] (EditListener& l) { l.modelChanged(); });
}

static juce::String asciiName (const std::vector<uint8_t>* data)
{
    if (data == nullptr || data->size() < 12)
        return {};

    juce::String s;
    for (int i = 0; i < 12; ++i)
    {
        const auto c = (*data)[(size_t) i];
        s += juce::juce_wchar (c >= 32 && c <= 126 ? c : ' ');
    }
    return s.trimEnd();
}

juce::String MC909EditorProcessor::getPadName (int pad) const
{
    const auto it = blocks.find (blockKeyFor (Block::rhythmNote, pad));
    return it == blocks.end() ? juce::String() : asciiName (&it->second);
}

juce::String MC909EditorProcessor::getKitName() const
{
    return asciiName (findBlock (Block::rhythmCommon));
}

void MC909EditorProcessor::playTestNote (int noteNumber, int velocity, bool on)
{
    const int channel = selectedPart + 1;
    hub.send (on ? juce::MidiMessage::noteOn  (channel, noteNumber, (juce::uint8) velocity)
                 : juce::MidiMessage::noteOff (channel, noteNumber));
}

void MC909EditorProcessor::detectDevice()
{
    hub.send (roland::makeIdentityRequest());
}

//==============================================================================
void MC909EditorProcessor::parameterChanged (const juce::String& paramId, float newValue)
{
    if (paramId.startsWith ("macro."))
    {
        const int slot = paramId.fromFirstOccurrenceOf (".", false, false).getIntValue() - 1;

        // Ignore the burst of values a host replays while loading a project, and the
        // value we set ourselves when a knob is (re)assigned.
        if (macroSuppress || juce::Time::getMillisecondCounter() < macroQuietUntil)
            return;

        if (juce::isPositiveAndBelow (slot, numMacros))
        {
            macroValue[slot].store (newValue);
            macroDirty.fetch_or (1u << slot);
        }
        return;
    }

    // Host automation goes out as Quick SysEx: four bytes shorter than DT1 and
    // able to address several tones at once, which is what the hardware's own
    // knobs use.
    for (const auto& ap : quick::automatable())
    {
        if (paramId == ap.id)
        {
            const uint8_t page = quick::kPatchPage + (uint8_t) selectedPart;
            hub.send (roland::makeQuickSysEx (deviceId,
                                              page,
                                              ap.offset,
                                              (uint8_t) juce::jlimit (0, 127, (int) newValue),
                                              (uint8_t) toneMask));
            return;
        }
    }
}

void MC909EditorProcessor::sysExReceived (const juce::MidiMessage& m)
{
    if (roland::isMC909IdentityReply (m))
    {
        const auto id = hub.currentInputId();
        editListeners.call ([&id] (EditListener& l) { l.deviceDetected (id); });
        return;
    }

    roland::DT1 dt;
    if (! roland::parseDT1 (m, dt))
    {
        ++sysexUnreadable;
        hub.logLine ("  -> not a valid DT1 for this model (wrong header or checksum)");
        return;
    }

    // Work out which block this belongs to by comparing absolute addresses.
    const Block candidates[] { Block::patchCommon, Block::patchTMT, Block::patchTone,
                               Block::partInfoPart, Block::compEQ,
                               Block::systemCommon, Block::mastering,
                               Block::rhythmCommon, Block::rhythmNote };

    const uint32_t incoming = dt.address.toLinear();

    const int savedTone = selectedTone, savedPad = selectedPad;
    bool matched = false;

    for (auto b : candidates)
    {
        const int count = (b == Block::patchTone) ? 4 : (b == Block::rhythmNote) ? (int) mc909::kRhythmPads : 1;

        for (int n = 0; n < count && ! matched; ++n)
        {
            if (b == Block::patchTone)  selectedTone = n;
            if (b == Block::rhythmNote) selectedPad  = n;

            const uint32_t base = blockAddress (b, selectedPart, selectedTone, selectedPad).toLinear();
            const uint32_t size = mc909::blockByteCount (b);

            if (incoming >= base && incoming < base + size)
            {
                auto& data = blockData (b);
                const uint32_t start = incoming - base;

                for (size_t i = 0; i < dt.data.size(); ++i)
                    if (start + i < data.size())
                        data[start + i] = dt.data[i];

                matched = true;
            }
        }

        if (matched)
            break;
    }

    selectedTone = savedTone;
    selectedPad  = savedPad;

    if (matched)
    {
        ++dt1Stored;
        editListeners.call ([] (EditListener& l) { l.modelChanged(); });
    }
    else
    {
        ++dt1Unplaced;
        hub.logLine ("  -> DT1 at address " + juce::String::toHexString (dt.address.b.data(), 4, 1)
                     + " does not fall inside any block this editor knows");
    }
}

//==============================================================================
void MC909EditorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("MC909EditorState");

    root.setProperty ("part",     selectedPart, nullptr);
    root.setProperty ("tone",     selectedTone, nullptr);
    root.setProperty ("pad",      selectedPad,  nullptr);
    root.setProperty ("toneMask", toneMask,     nullptr);
    root.setProperty ("deviceId", (int) deviceId, nullptr);
    root.setProperty ("midiOut",  hub.currentOutputId(), nullptr);
    root.setProperty ("midiIn",   hub.currentInputId(),  nullptr);

    juce::ValueTree blocksTree ("Blocks");
    for (const auto& [key, data] : blocks)
    {
        juce::ValueTree b ("Block");
        b.setProperty ("key", key, nullptr);
        b.setProperty ("data", juce::Base64::toBase64 (data.data(), data.size()), nullptr);
        blocksTree.appendChild (b, nullptr);
    }
    root.appendChild (blocksTree, nullptr);

    juce::ValueTree macros ("Macros");
    for (int n = 0; n < numMacros; ++n)
        macros.setProperty ("m" + juce::String (n), macroDef[n] != nullptr ? macroDef[n]->id : juce::String(), nullptr);
    root.appendChild (macros, nullptr);
    root.appendChild (valueTree.copyState(), nullptr);

    juce::MemoryOutputStream stream (destData, false);
    root.writeToStream (stream);
}

void MC909EditorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto root = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! root.isValid())
        return;

    selectedPart = root.getProperty ("part", 0);
    selectedTone = root.getProperty ("tone", 0);
    selectedPad  = juce::jlimit (0, 15, (int) root.getProperty ("pad", 1));
    toneMask     = root.getProperty ("toneMask", quick::kAllTones);
    deviceId     = (uint8_t) (int) root.getProperty ("deviceId", 0x10);

    blocks.clear();
    if (auto blocksTree = root.getChildWithName ("Blocks"); blocksTree.isValid())
    {
        for (const auto& b : blocksTree)
        {
            juce::MemoryOutputStream out;
            if (juce::Base64::convertFromBase64 (out, b.getProperty ("data").toString()))
            {
                const auto* p = (const uint8_t*) out.getData();
                blocks[b.getProperty ("key").toString()] =
                    std::vector<uint8_t> (p, p + out.getDataSize());
            }
        }
    }

    macroQuietUntil = juce::Time::getMillisecondCounter() + 2000;
    macroDirty = 0;

    if (auto params = root.getChildWithName (valueTree.state.getType()); params.isValid())
        valueTree.replaceState (params);

    {
        auto macros = root.getChildWithName ("Macros");
        for (int n = 0; n < numMacros; ++n)
        {
            const mc909::ParamDef* found = nullptr;
            const auto id = macros.getProperty ("m" + juce::String (n)).toString();
            if (id.isNotEmpty())
                for (const auto& d : allParams())
                    if (d.id == id) { found = &d; break; }

            macroDef[n] = found;
            macroLastRaw[n] = -1;
            if (auto* m = dynamic_cast<MacroParam*> (getMacroParameter (n)))
                m->setLabel (found != nullptr ? found->name : juce::String());
        }
        updateHostDisplay (ChangeDetails().withParameterInfoChanged (true));
    }

    hub.openOutput (root.getProperty ("midiOut", "").toString());
    hub.openInput  (root.getProperty ("midiIn",  "").toString());

    editListeners.call ([] (EditListener& l) { l.modelChanged(); });
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MC909EditorProcessor();
}
