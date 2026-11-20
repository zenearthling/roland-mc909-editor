#include "SvdFile.h"
#include "SvdTables.h"
#include <cstring>

namespace svd
{

bool File::load (const uint8_t* data, size_t size, std::string* error)
{
    bytes.clear();

    if (size < (size_t) (kPatOffset + kNumSlots * kItemBytes))
    {
        if (error) *error = "file is too small to be an MC-909 snd909.svd";
        return false;
    }

    static const uint8_t magic[] = { 0x00, 0x7E, 'S', 'V', 'D', '1' };
    if (std::memcmp (data, magic, sizeof magic) != 0)
    {
        if (error) *error = "not an MC-909 .svd file (header is wrong)";
        return false;
    }

    bytes.assign (data, data + size);
    return true;
}

uint32_t File::bits (int slot, int bitPos, int width) const
{
    const size_t base = (size_t) kPatOffset + (size_t) (slot - 1) * (size_t) kItemBytes;
    uint32_t v = 0;
    for (int i = 0; i < width; ++i)
    {
        const int p = bitPos + i;
        const uint8_t b = bytes[base + (size_t) (p >> 3)];
        v = (v << 1) | (uint32_t) ((b >> (7 - (p & 7))) & 1);
    }
    return v;
}

std::string File::name (int slot) const
{
    if (! isLoaded() || slot < 1 || slot > kNumSlots)
        return {};

    std::string s;
    for (int i = 0; i < 12; ++i)
    {
        const int c = (int) bits (slot, i * 7, 7);
        s += (c >= 32 && c < 127) ? (char) c : ' ';
    }
    while (! s.empty() && s.back() == ' ')
        s.pop_back();
    return s;
}

namespace
{
    template <size_t N>
    const Field* findField (const Field (&table)[N], const char* name)
    {
        for (const auto& f : table)
            if (std::strcmp (f.name, name) == 0)
                return &f;
        return nullptr;
    }

    void putNibbles (std::vector<uint8_t>& out, int offset, int nibbles, uint32_t v)
    {
        for (int i = 0; i < nibbles; ++i)
            out[(size_t) (offset + i)] = (uint8_t) ((v >> (4 * (nibbles - 1 - i))) & 0x0F);
    }
}

int File::value (int slot, Section s, int tone, const char* field) const
{
    if (! isLoaded() || slot < 1 || slot > kNumSlots)
        return -1;

    const Field* f = s == Section::common ? findField (kCommonFields, field)
                   : s == Section::tmt    ? findField (kTMTFields, field)
                                          : findField (kToneFields, field);
    if (f == nullptr)
        return -1;

    const int shift = (s == Section::tone) ? tone * kToneStride : 0;
    return (int) bits (slot, f->bit + shift, f->width) + f->bias;
}

PatchBlocks File::blocks (int slot) const
{
    PatchBlocks pb;
    if (! isLoaded() || slot < 1 || slot > kNumSlots)
        return pb;

    auto build = [&] (const Field* table, int count, int size, int shift)
    {
        std::vector<uint8_t> out ((size_t) size, 0);
        for (int i = 0; i < count; ++i)
        {
            const Field& f = table[i];
            const uint32_t v = bits (slot, f.bit + shift, f.width) + (uint32_t) f.bias;
            if (f.nibbles == 1)
                out[f.offset] = (uint8_t) (v & 0x7F);
            else
                putNibbles (out, f.offset, f.nibbles, v);
        }
        return out;
    };

    pb.common = build (kCommonFields, kCommonCount, kCommonBytes, 0);
    pb.tmt    = build (kTMTFields,    kTMTCount,    kTMTBytes,    0);
    for (int t = 0; t < 4; ++t)
        pb.tone[(size_t) t] = build (kToneFields, kToneCount, kToneBytes, t * kToneStride);
    return pb;
}

} // namespace svd
