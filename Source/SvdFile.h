#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

/** Reader for the MC-909's snd909.svd user-patch file (USER drive, and the same file on the CARD).
    No JUCE, so it can be tested on its own. Layout and field tables: see SvdTables.h and
    docs / mc909-svd-format-notes. 256 patch items of 598 bytes, bit packed MSB first. */
namespace svd
{

enum class Section { common, tmt, tone };

/** One patch as the byte images the unit's SysEx blocks use (Patch Common, TMT, four Tone blocks). */
struct PatchBlocks
{
    std::vector<uint8_t> common, tmt;
    std::array<std::vector<uint8_t>, 4> tone;
};

class File
{
public:
    /** Takes a copy. False with `error` set when this is not a plausible snd909.svd. */
    bool load (const uint8_t* data, size_t size, std::string* error = nullptr);

    bool isLoaded() const { return ! bytes.empty(); }

    /** Patch name of slot 1-256, trailing spaces removed. */
    std::string name (int slot) const;

    /** Raw SysEx value of a field (stored bits plus bias), e.g. tvf_cutoff_frequency of tone 0-3.
        -1 when the field name is unknown. */
    int value (int slot, Section s, int tone, const char* field) const;

    /** The unit's SysEx block images for this patch. */
    PatchBlocks blocks (int slot) const;

private:
    uint32_t bits (int slot, int bitPos, int width) const;
    std::vector<uint8_t> bytes;
};

} // namespace svd
