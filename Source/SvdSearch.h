#pragma once
#include "SearchIndex.h"
#include "SvdFile.h"

namespace svd
{
/** Search entry for one patch of a snd909.svd: name, "USER 201", searchable wave names, and the
    detail pane text (tones, filter, levels). `bank` is "USER" or "CARD". */
mc909search::DynamicEntry makeSearchEntry (const File&, int slot, const char* bank);

constexpr int kSlotCount = 256;
}
