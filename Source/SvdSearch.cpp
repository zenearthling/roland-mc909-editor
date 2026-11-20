#include "SvdSearch.h"
#include <cstdio>
#include <string>

namespace svd
{

namespace
{
    const char* filterTypeName (int v)
    {
        static const char* n[] = { "OFF", "LPF", "BPF", "HPF", "PKG", "LPF2", "LPF3" };
        return (v >= 0 && v < 7) ? n[v] : "?";
    }
}

mc909search::DynamicEntry makeSearchEntry (const File& f, int slot, const char* bank)
{
    using S = Section;
    mc909search::DynamicEntry d;
    d.number = slot;
    d.name = f.name (slot);

    char where[32];
    std::snprintf (where, sizeof where, "%s %03d", bank, slot);
    d.where = where;

    const bool mono = f.value (slot, S::common, 0, "mono_poly") == 0;
    d.detail = "# Patch\n";
    d.detail += std::string ("Slot\t") + where + "\n";
    d.detail += std::string ("Voice\t") + (mono ? "Mono" : "Poly")
              + (f.value (slot, S::common, 0, "legato_switch") == 1 ? ", legato" : "")
              + (f.value (slot, S::common, 0, "portamento_switch") == 1 ? ", portamento" : "") + "\n";
    d.detail += "Level\t" + std::to_string (f.value (slot, S::common, 0, "patch_level")) + "\n";

    d.extra = mono ? "mono" : "poly";
    for (int t = 0; t < 4; ++t)
    {
        const std::string sw = "tmt" + std::to_string (t + 1) + "_tone_switch";
        if (f.value (slot, S::tmt, 0, sw.c_str()) != 1)
            continue;

        const int group = f.value (slot, S::tone, t, "wave_group_type");
        const int wave  = f.value (slot, S::tone, t, "wave_number_l");
        std::string waveText;
        if (group == 0)
        {
            const char* wn = mc909search::waveformName (wave);
            waveText = wn != nullptr ? std::string (wn) + " (" + std::to_string (wave) + ")"
                                     : "waveform " + std::to_string (wave);
            if (wn != nullptr)
                d.extra += std::string ("|") + wn;
        }
        else
            waveText = "wave group " + std::to_string (group) + ", " + std::to_string (wave);

        d.detail += "# Tone " + std::to_string (t + 1) + "\n";
        d.detail += "Wave\t" + waveText + "\n";
        d.detail += std::string ("Filter\t") + filterTypeName (f.value (slot, S::tone, t, "tvf_filter_type"))
                  + "  cutoff " + std::to_string (f.value (slot, S::tone, t, "tvf_cutoff_frequency"))
                  + "  reso " + std::to_string (f.value (slot, S::tone, t, "tvf_resonance")) + "\n";
        d.detail += "Level\t" + std::to_string (f.value (slot, S::tone, t, "tone_level")) + "\n";
    }

    d.detail += "# Note\nLoads the tones, TMT and common settings. The patch's effects are not read from the file yet, "
                "so the Part's effects stay as they are.\n";
    return d;
}

} // namespace svd
