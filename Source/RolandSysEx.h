#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace roland
{

// ---------------------------------------------------------------------------
// Roland addresses are four 7-bit bytes. Arithmetic is therefore base-128,
// NOT base-256. Getting this wrong is the classic reason a hand-rolled Roland
// editor "almost works": Part 1 is 11 00 00 00 and Part 16 is 14 60 00 00,
// which only comes out right if you carry at 128.
// ---------------------------------------------------------------------------
struct Address
{
    std::array<uint8_t, 4> b { { 0, 0, 0, 0 } };

    Address() = default;
    Address (uint8_t a0, uint8_t a1, uint8_t a2, uint8_t a3) : b { { a0, a1, a2, a3 } } {}

    /** Flatten to a plain integer in base-128 space. */
    uint32_t toLinear() const noexcept
    {
        return (uint32_t) b[0] * 128u * 128u * 128u
             + (uint32_t) b[1] * 128u * 128u
             + (uint32_t) b[2] * 128u
             + (uint32_t) b[3];
    }

    static Address fromLinear (uint32_t v) noexcept
    {
        Address a;
        a.b[3] = (uint8_t) (v % 128); v /= 128;
        a.b[2] = (uint8_t) (v % 128); v /= 128;
        a.b[1] = (uint8_t) (v % 128); v /= 128;
        a.b[0] = (uint8_t) (v % 128);
        return a;
    }

    Address operator+ (uint32_t offset) const noexcept
    {
        return fromLinear (toLinear() + offset);
    }

    Address operator+ (const Address& other) const noexcept
    {
        return fromLinear (toLinear() + other.toLinear());
    }

    bool operator== (const Address& o) const noexcept { return b == o.b; }

    juce::String toHexString() const
    {
        return juce::String::formatted ("%02X %02X %02X %02X", b[0], b[1], b[2], b[3]);
    }
};

/** Roland checksum: the low 7 bits of (address + data + checksum) must be zero. */
uint8_t checksum (const uint8_t* data, size_t numBytes) noexcept;

/** Split a value into n nibbles (MSN first), as used by the "#" marked
    multi-byte parameters in the address map. n is 1, 2 or 4. */
void toNibbles (int value, int numBytes, std::vector<uint8_t>& out);

/** Reassemble n nibbles back into a value. */
int fromNibbles (const uint8_t* data, int numBytes) noexcept;

// ---------------------------------------------------------------------------
// Message construction. deviceId is 0x10-0x1F (unit 1-16) or 0x7F (broadcast).
// modelId for the MC-909 is { 0x00, 0x59 }; the "Quick SysEx" model is { 0x5D }.
// ---------------------------------------------------------------------------
juce::MidiMessage makeDT1 (uint8_t deviceId,
                           const Address& addr,
                           const uint8_t* data,
                           size_t numDataBytes);

juce::MidiMessage makeRQ1 (uint8_t deviceId,
                           const Address& addr,
                           uint32_t sizeLinear);

/** Quick SysEx (model ID 5D) — two-byte address, two data bytes, used by the
    hardware for fast realtime knob-style edits across multiple tones at once. */
juce::MidiMessage makeQuickSysEx (uint8_t deviceId,
                                  uint8_t addrMsb, uint8_t addrLsb,
                                  uint8_t data0, uint8_t data1);

/** Universal identity request — the MC-909 replies with
    F0 7E dev 06 02 41 59 01 00 00 ... F7, which is how we auto-detect it. */
juce::MidiMessage makeIdentityRequest();

struct DT1
{
    uint8_t  deviceId = 0;
    Address  address;
    std::vector<uint8_t> data;
};

/** Returns true and fills 'out' if the message is a well-formed MC-909 DT1
    with a valid checksum. */
bool parseDT1 (const juce::MidiMessage& m, DT1& out);

/** Returns true if the message is an Identity Reply that looks like an MC-909. */
bool isMC909IdentityReply (const juce::MidiMessage& m);

} // namespace roland
