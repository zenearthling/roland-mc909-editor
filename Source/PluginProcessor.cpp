#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mc909;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MC909EditorProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Only the Quick SysEx set is exposed for host automation. Exposing all
    // ~900 addressable parameters would swamp the host's automation list and
    // saturate the MIDI link; these twelve are the ones worth a fader.
    for (const auto& ap : quick::automatable())
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { ap.id, 1 }, ap.name, 0, 127, 64));

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

    // Allocate the local mirror so the UI has something coherent before the
    // first dump arrives.
    const Block all[] { Block::patchCommon, Block::patchTMT, Block::patchTone,
                        Block::partInfoPart, Block::compEQ,
                        Block::systemCommon, Block::mastering };
    for (auto b : all)
        blockData (b).assign (mc909::blockByteCount (b), 0);
}

MC909EditorProcessor::~MC909EditorProcessor()
{
    for (const auto& ap : quick::automatable())
        valueTree.removeParameterListener (ap.id, this);

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
    }
    return "unknown";
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
        return readValue (*data, p.offset, p.numBytes);

    return p.rawMin;
}

void MC909EditorProcessor::setParameterValue (const ParamDef& p, int rawValue, bool sendToDevice)
{
    rawValue = juce::jlimit (p.rawMin, p.rawMax, rawValue);

    std::vector<uint8_t> bytes;
    roland::toNibbles (rawValue, p.numBytes, bytes);

    auto& data = blockData (p.block);
    for (size_t i = 0; i < bytes.size() && p.offset + i < data.size(); ++i)
        data[p.offset + i] = bytes[i];

    if (sendToDevice)
    {
        const auto addr = blockAddress (p.block, selectedPart, selectedTone) + p.offset;
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
    const auto addr = blockAddress (b, selectedPart, selectedTone);
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
}

void MC909EditorProcessor::probePatchSizes()
{
    const int gapMs = 300;
    hub.logLine ("--- PROBE: patch block sizes, part " + juce::String (selectedPart + 1) + " ---");

    auto probe = [this, gapMs] (Block b, std::initializer_list<uint32_t> sizes)
    {
        const auto addr = blockAddress (b, selectedPart, 0);
        for (auto sz : sizes)
            hub.send (roland::makeRQ1 (deviceId, addr, sz), gapMs);
    };

    probe (Block::patchCommon, { 0x01, 0x0C, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x56, 0x58, 0x5A,
                                 0x5C, 0x60, 0x64, 0x68, 0x70, 0x7F, 0x80 });

    probe (Block::patchTMT,    { 0x01, 0x0C, 0x20, 0x25, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x30, 0x34,
                                 0x38, 0x40, 0x50, 0x7F, 0x80 });

    probe (Block::patchTone,   { 0x01, 0x0C, 0x40, 0x7F, 0x80, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x90,
                                 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x100, 0x108, 0x10B, 0x120,
                                 0x140, 0x180, 0x200 });
}

void MC909EditorProcessor::sendAll()
{
    std::vector<juce::MidiMessage> out;

    auto pushBlock = [&] (Block b)
    {
        const auto& data = blockData (b);
        const auto base  = blockAddress (b, selectedPart, selectedTone);

        // The device wants packets of 256 bytes or fewer; 128 keeps us clear of
        // the base-128 address boundary too, which makes the maths trivial.
        constexpr size_t chunk = 128;
        for (size_t pos = 0; pos < data.size(); pos += chunk)
        {
            const size_t n = juce::jmin (chunk, data.size() - pos);
            out.push_back (roland::makeDT1 (deviceId, base + (uint32_t) pos, data.data() + pos, n));
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
                               Block::systemCommon, Block::mastering };

    const uint32_t incoming = dt.address.toLinear();

    const int savedTone = selectedTone;
    bool matched = false;

    for (auto b : candidates)
    {
        const int toneLoop = (b == Block::patchTone) ? 4 : 1;

        for (int t = 0; t < toneLoop && ! matched; ++t)
        {
            if (b == Block::patchTone)
                selectedTone = t;

            const uint32_t base = blockAddress (b, selectedPart, selectedTone).toLinear();
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

        selectedTone = savedTone;
    }

    selectedTone = savedTone;

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

    if (auto params = root.getChildWithName (valueTree.state.getType()); params.isValid())
        valueTree.replaceState (params);

    hub.openOutput (root.getProperty ("midiOut", "").toString());
    hub.openInput  (root.getProperty ("midiIn",  "").toString());

    editListeners.call ([] (EditListener& l) { l.modelChanged(); });
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MC909EditorProcessor();
}
