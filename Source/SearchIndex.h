#pragma once
#include <cstdint>
#include <string>
#include <vector>

/** Search over everything printed in the MC-909 patch list appendix.

    Deliberately free of JUCE so the ranking can be tested on its own. The data
    table (SearchData.cpp) is generated from Roland_MC-909_Patch_List.pdf by
    tools/gen_search_data.py. */
namespace mc909search
{

enum class Kind : uint8_t
{
    waveform,     ///< internal waveform, 1-693
    patch,        ///< preset patch, banks A-G
    rhythmKit,    ///< preset rhythm set, 1-72
    pattern,      ///< preset sequencer pattern
    patternSet,
    rpsPattern,   ///< RPS (realtime phrase sequence) pattern
    rpsSet
};

constexpr int kNumKinds = 7;
constexpr uint32_t kAllKinds = (1u << kNumKinds) - 1u;

inline uint32_t kindBit (Kind k) { return 1u << (unsigned) k; }

const char* kindLabel (Kind);          ///< "Waveform"
const char* kindLabelPlural (Kind);    ///< "Waveforms"

struct Entry
{
    Kind kind;
    const char* name;     ///< what you'd type to find it, e.g. "TR909 Kick 7"
    const char* where;    ///< "Preset A 042", "Waveform 504" ...
    const char* extra;    ///< more searchable text, '|' separated (kit contents, programmer, BPM)
    const char* detail;   ///< display only: lines of "label<TAB>value"; "# " starts a heading
    int number;           ///< waveform number / program / kit / pattern number
    int bank;             ///< patches: 0-6 for A-G, otherwise 0
};

extern const Entry kEntries[];
extern const int   kNumEntries;

struct Hit
{
    int index = 0;        ///< into kEntries
    int score = 0;
    int extraSegment = -1;///< which '|' segment of `extra` matched best (-1: matched the name/where)
};

struct Range { int start = 0, length = 0; };

class Searcher
{
public:
    Searcher();

    /** All entries matching every word of `query`, best first. An empty query
        returns everything in table order. Words match anywhere in the name,
        the location text, or the extra text; name matches rank highest and
        matches at the start of a word beat matches in the middle of one. */
    std::vector<Hit> search (const std::string& query) const;

    /** Characters of the entry's name that matched the query, merged and sorted. */
    std::vector<Range> highlight (int entryIndex, const std::string& query) const;

    /** One '|' separated piece of an entry's extra text. */
    std::string extraSegment (int entryIndex, int segment) const;

    /** Characters of that extra piece that matched the query. */
    std::vector<Range> highlightExtra (int entryIndex, int segment, const std::string& query) const;

    static std::vector<std::string> tokenise (const std::string& query);

private:
    struct Cached
    {
        std::string lname, lcompact, lwhere;
        std::vector<std::string> lextra;
        std::vector<bool> wordStart;   // per character of the name
    };
    std::vector<Cached> cache;
};

} // namespace mc909search
