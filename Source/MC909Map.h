#pragma once
#include "RolandSysEx.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace mc909
{

// ---------------------------------------------------------------------------
// Block base addresses, straight from the published MC-909 MIDI implementation
// (model ID 00 59). All offsets below are *linear* base-128 offsets; convert
// with roland::Address::fromLinear when you need the wire bytes.
// ---------------------------------------------------------------------------
inline roland::Address setupBase()     { return { 0x01, 0x00, 0x00, 0x00 }; }
inline roland::Address systemBase()    { return { 0x02, 0x00, 0x00, 0x00 }; }
inline roland::Address partInfoBase()  { return { 0x10, 0x00, 0x00, 0x00 }; }

constexpr uint32_t kSystemCommon      = 0;            // 00 00 00
constexpr uint32_t kSystemMastering   = 2 * 128;      // 00 02 00
constexpr uint32_t kSystemPart0       = 0x10 * 128;   // 00 10 00
constexpr uint32_t kSystemPartStride  = 128;
constexpr uint32_t kSystemController  = 0x40 * 128;   // 00 40 00

constexpr uint32_t kPartInfoCommon    = 0;
constexpr uint32_t kPartInfoMFX1      = 2 * 128;
constexpr uint32_t kPartInfoMFX2      = 4 * 128;
constexpr uint32_t kPartInfoReverb    = 6 * 128;
constexpr uint32_t kPartInfoCompEQ    = 8 * 128;
constexpr uint32_t kPartInfoExtIn     = 10 * 128;
constexpr uint32_t kPartInfoPart0     = 0x20 * 128;   // 00 20 00
constexpr uint32_t kPartInfoPartStride = 128;

/** Temporary Patch/Rhythm for a part. Part 1 = 11 00 00 00, part 16 = 14 60 00 00. */
inline roland::Address temporaryPart (int part /* 0-15 */)
{
    const roland::Address base { 0x11, 0x00, 0x00, 0x00 };
    return base + (uint32_t) part * (0x20u * 128u * 128u);
}

constexpr uint32_t kTempPatch         = 0;             // 00 00 00
constexpr uint32_t kTempRhythm        = 0x10u * 128u * 128u; // 10 00 00

constexpr uint32_t kPatchCommon       = 0;
constexpr uint32_t kPatchTMT          = 0x10 * 128;    // 00 10 00
constexpr uint32_t kPatchTone0        = 0x20 * 128;    // 00 20 00
constexpr uint32_t kPatchToneStride   = 2 * 128;       // 00 02 00

constexpr uint32_t kRhythmCommon      = 0;
constexpr uint32_t kRhythmTone0       = 0x5C * 128;    // key #59
constexpr uint32_t kRhythmToneStride  = 2 * 128;

// Block sizes (linear), used for RQ1 requests.
constexpr uint32_t kSizePatchCommon   = 0x51;
constexpr uint32_t kSizePatchTMT      = 0x29;
constexpr uint32_t kSizePatchTone     = 128 + 0x0B;    // 00 00 01 0B
constexpr uint32_t kSizePartInfoPart  = 0x0C;
constexpr uint32_t kSizeSystemCommon  = 0x0A;
constexpr uint32_t kSizeMastering     = 0x12;
constexpr uint32_t kSizeCompEQ        = 0x0C;

// ---------------------------------------------------------------------------

enum class Block
{
    patchCommon,
    patchTMT,
    patchTone,
    partInfoPart,
    compEQ,
    systemCommon,
    mastering
};

struct ParamDef
{
    juce::String id;          ///< stable, e.g. "tone.tvf_cutoff"
    juce::String name;        ///< display name
    Block        block   = Block::patchTone;
    uint32_t     offset  = 0; ///< linear offset inside the block
    int          numBytes = 1;///< 1 = plain, 2/4 = nibbled
    int          rawMin  = 0;
    int          rawMax  = 127;
    int          dispSub = 0; ///< display value = raw - dispSub
    int          dispMul = 1; ///< ...then multiplied by this
    juce::StringArray choices; ///< non-empty => render as a combo box

    bool isChoice() const noexcept { return choices.size() > 0; }
    int  toDisplay (int raw) const noexcept { return (raw - dispSub) * dispMul; }
    int  fromDisplay (int d) const noexcept { return d / dispMul + dispSub; }
};

/** The full table, built once. */
const std::vector<ParamDef>& allParams();

/** All parameters belonging to one block, in address order. */
std::vector<const ParamDef*> paramsForBlock (Block b);

/** Total byte length of a block (for RQ1 / bulk parsing). */
uint32_t blockSize (Block b);

/** Resolve a block to an absolute address for the given part / tone. */
roland::Address blockAddress (Block b, int part /* 0-15 */, int tone /* 0-3 */);

// ---------------------------------------------------------------------------
// Quick SysEx (model ID 5D) — the compact realtime path the hardware itself
// uses for knob moves. One message sets one parameter across any combination
// of the four tones, which makes it the right transport for host automation.
// ---------------------------------------------------------------------------
namespace quick
{
    constexpr uint8_t kPatchPage     = 0x00;
    constexpr uint8_t kRhythmPage    = 0x20;
    constexpr uint8_t kSequencerPage = 0x40;

    constexpr uint8_t kPan          = 0x01;
    constexpr uint8_t kRandomPan    = 0x02;
    constexpr uint8_t kPitchEnvDep  = 0x05;
    constexpr uint8_t kTvfType      = 0x0A;
    constexpr uint8_t kCutoff       = 0x0B;
    constexpr uint8_t kResonance    = 0x0C;
    constexpr uint8_t kTvfEnvDepth  = 0x0D;
    constexpr uint8_t kTvfAttack    = 0x0E;
    constexpr uint8_t kTvfDecay     = 0x0F;
    constexpr uint8_t kTvfSustain   = 0x10;
    constexpr uint8_t kTvfRelease   = 0x11;
    constexpr uint8_t kTvaAttack    = 0x12;
    constexpr uint8_t kTvaDecay     = 0x13;
    constexpr uint8_t kTvaSustain   = 0x14;
    constexpr uint8_t kTvaRelease   = 0x15;
    constexpr uint8_t kLfoWave      = 0x16;
    constexpr uint8_t kLfoRate      = 0x17;
    constexpr uint8_t kLfoPitchDep  = 0x18;
    constexpr uint8_t kLfoTvfDep    = 0x19;
    constexpr uint8_t kLfoTvaDep    = 0x1A;

    constexpr uint8_t kAllTones = 0x0F;

    struct AutoParam { const char* id; const char* name; uint8_t offset; };

    /** The curated set exposed to the host as automatable parameters. */
    const std::vector<AutoParam>& automatable();
}

} // namespace mc909
