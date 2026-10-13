#include "SearchIndex.h"
#include <algorithm>
#include <bitset>
#include <cctype>

namespace mc909search
{

const char* kindLabel (Kind k)
{
    switch (k)
    {
        case Kind::waveform:   return "Waveform";
        case Kind::patch:      return "Patch";
        case Kind::rhythmKit:  return "Rhythm Kit";
        case Kind::pattern:    return "Pattern";
        case Kind::patternSet: return "Pattern Set";
        case Kind::rpsPattern: return "RPS Pattern";
        case Kind::rpsSet:     return "RPS Set";
    }
    return "";
}

const char* kindLabelPlural (Kind k)
{
    switch (k)
    {
        case Kind::waveform:   return "Waveforms";
        case Kind::patch:      return "Patches";
        case Kind::rhythmKit:  return "Rhythm Kits";
        case Kind::pattern:    return "Patterns";
        case Kind::patternSet: return "Pattern Sets";
        case Kind::rpsPattern: return "RPS Patterns";
        case Kind::rpsSet:     return "RPS Sets";
    }
    return "";
}

namespace
{
    std::string lower (const std::string& s)
    {
        std::string r (s);
        for (auto& c : r)
            c = (char) std::tolower ((unsigned char) c);
        return r;
    }

    bool isAlpha (char c) { return std::isalpha ((unsigned char) c) != 0; }
    bool isAlnum (char c) { return std::isalnum ((unsigned char) c) != 0; }

    /** A character starts a "word" if it follows a non-alphanumeric, a lower->UPPER
        step (SuperSaw), or a letter<->digit step (TR909). So "909" finds TR909 and
        "saw" finds SuperSaw as word starts, which is what people expect. */
    std::vector<bool> wordStarts (const std::string& s)
    {
        std::vector<bool> w (s.size(), false);
        for (size_t i = 0; i < s.size(); ++i)
        {
            if (i == 0 || ! isAlnum (s[i - 1]))
                w[i] = isAlnum (s[i]);
            else if (std::islower ((unsigned char) s[i - 1]) && std::isupper ((unsigned char) s[i]))
                w[i] = true;
            else if (isAlpha (s[i - 1]) != isAlpha (s[i]) && isAlnum (s[i]))
                w[i] = true;
        }
        return w;
    }

    std::vector<std::string> splitSegments (const std::string& s)
    {
        std::vector<std::string> out;
        if (s.empty())
            return out;
        size_t a = 0;
        for (;;)
        {
            const size_t b = s.find ('|', a);
            out.push_back (s.substr (a, b == std::string::npos ? std::string::npos : b - a));
            if (b == std::string::npos)
                break;
            a = b + 1;
        }
        return out;
    }

    /** Best occurrence of `tok` in `hay`: a word-start hit beats an earlier mid-word one.
        Returns -1 if absent, else the position; `atWordStart` says which kind it was. */
    int bestFind (const std::string& hay, const std::string& tok, const std::vector<bool>* starts,
                  bool& atWordStart)
    {
        int first = -1;
        for (size_t p = hay.find (tok); p != std::string::npos; p = hay.find (tok, p + 1))
        {
            const bool ws = (p == 0) || (starts != nullptr ? (*starts)[p] : ! isAlnum (hay[p - 1]));
            if (first < 0)
            {
                first = (int) p;
                atWordStart = ws;
            }
            if (ws)
            {
                atWordStart = true;
                return (int) p;
            }
        }
        return first;
    }
}

//==============================================================================
std::vector<std::string> Searcher::tokenise (const std::string& query)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : query)
    {
        if (std::isspace ((unsigned char) c))
        {
            if (! cur.empty()) { out.push_back (lower (cur)); cur.clear(); }
        }
        else
            cur += c;
    }
    if (! cur.empty())
        out.push_back (lower (cur));
    return out;
}

Searcher::Searcher()
{
    cache.resize ((size_t) kNumEntries);
    for (int i = 0; i < kNumEntries; ++i)
    {
        const auto& e = kEntries[i];
        auto& c = cache[(size_t) i];
        c.lname = lower (e.name);
        for (char ch : c.lname)
            if (! std::isspace ((unsigned char) ch))
                c.lcompact += ch;
        c.lwhere = lower (e.where);
        for (auto& seg : splitSegments (e.extra))
            c.lextra.push_back (lower (seg));
        c.wordStart = wordStarts (e.name);
    }
}

std::vector<Hit> Searcher::search (const std::string& query) const
{
    auto tokens = tokenise (query);
    if (tokens.size() > 16)
        tokens.resize (16);                 // masks below are 32-bit; nobody types more

    std::vector<Hit> hits;
    hits.reserve ((size_t) kNumEntries);

    std::string joined;
    for (auto& t : tokens)
        joined += (joined.empty() ? "" : " ") + t;

    for (int i = 0; i < kNumEntries; ++i)
    {
        const auto& c = cache[(size_t) i];
        int total = 0;
        uint32_t needMask = 0;              // tokens the name did not satisfy
        int whereScore[16] {};

        // 1. the name. "kick7" should still find "Kick 7", hence the compact form.
        for (size_t ti = 0; ti < tokens.size(); ++ti)
        {
            const auto& tok = tokens[ti];
            bool ws = false;
            if (const int p = bestFind (c.lname, tok, &c.wordStart, ws); p >= 0)
                total += (p == 0) ? 100 : (ws ? 80 : 50);
            else if (c.lcompact.find (tok) != std::string::npos)
                total += 45;
            else
            {
                needMask |= 1u << ti;
                if (const int p2 = bestFind (c.lwhere, tok, nullptr, ws); p2 >= 0)
                    whereScore[ti] = ws ? 35 : 30;      // "preset a", "504"
            }
        }

        bool matched = true;
        int bestExtra = -1;

        // 2. whatever the name could not satisfy has to be found in the location
        //    text or the extra text (kit contents, programmer, tempo).
        if (needMask != 0)
        {
            int extraScore[16] {};
            int bestCover = 0;
            uint32_t bestMask = 0;

            for (size_t s = 0; s < c.lextra.size(); ++s)
            {
                uint32_t m = 0;
                for (size_t ti = 0; ti < tokens.size(); ++ti)
                {
                    if ((needMask & (1u << ti)) == 0)
                        continue;
                    bool ws = false;
                    if (bestFind (c.lextra[s], tokens[ti], nullptr, ws) >= 0)
                    {
                        m |= 1u << ti;
                        extraScore[ti] = std::max (extraScore[ti], ws ? 28 : 25);
                    }
                }
                const int cover = (int) std::bitset<32> (m).count();
                if (cover > bestCover)          // first segment wins ties
                {
                    bestCover = cover;
                    bestMask = m;
                    bestExtra = (int) s;
                }
            }

            for (size_t ti = 0; ti < tokens.size() && matched; ++ti)
            {
                if ((needMask & (1u << ti)) == 0)
                    continue;
                const int sc = std::max (whereScore[ti], extraScore[ti]);
                if (sc == 0) matched = false;
                total += sc;
            }

            // A kit with ONE pad called "TR909 Kick 7" is a better answer to
            // "tr909 kick 7" than a kit that has those words on different pads.
            if (matched && std::bitset<32> (needMask).count() >= 2 && bestMask == needMask)
                total += 40;
        }

        if (! matched)
            continue;

        if (! tokens.empty())
        {
            if (needMask == 0)                            total += 40;   // everything in the name
            if (c.lname == joined)                        total += 300;
            else if (c.lname.compare (0, joined.size(), joined) == 0) total += 60;
            total -= std::min (30, (int) c.lname.size() / 3);           // shorter names first
        }

        hits.push_back ({ i, total, bestExtra });
    }

    // Best score first; ties keep the table order (kind, then number), which is
    // also what an empty query shows.
    std::stable_sort (hits.begin(), hits.end(),
                      [] (const Hit& a, const Hit& b) { return a.score > b.score; });
    return hits;
}

//==============================================================================
static std::vector<Range> mergeRanges (std::vector<Range> r)
{
    std::sort (r.begin(), r.end(), [] (const Range& a, const Range& b) { return a.start < b.start; });
    std::vector<Range> out;
    for (auto& x : r)
    {
        if (! out.empty() && x.start <= out.back().start + out.back().length)
            out.back().length = std::max (out.back().length, x.start + x.length - out.back().start);
        else
            out.push_back (x);
    }
    return out;
}

std::vector<Range> Searcher::highlight (int entryIndex, const std::string& query) const
{
    std::vector<Range> r;
    if (entryIndex < 0 || entryIndex >= kNumEntries)
        return r;

    const auto& c = cache[(size_t) entryIndex];
    for (const auto& tok : tokenise (query))
    {
        bool ws = false;
        if (const int p = bestFind (c.lname, tok, &c.wordStart, ws); p >= 0)
            r.push_back ({ p, (int) tok.size() });
        else if (const size_t q = c.lcompact.find (tok); q != std::string::npos)
        {
            // Map a hit in the space-less copy back onto the real name.
            int seen = 0, start = -1, end = -1;
            for (size_t k = 0; k < c.lname.size(); ++k)
            {
                if (std::isspace ((unsigned char) c.lname[k]))
                    continue;
                if (seen == (int) q)                    start = (int) k;
                if (seen == (int) (q + tok.size()) - 1) { end = (int) k; break; }
                ++seen;
            }
            if (start >= 0 && end >= start)
                r.push_back ({ start, end - start + 1 });
        }
    }
    return mergeRanges (std::move (r));
}

std::string Searcher::extraSegment (int entryIndex, int segment) const
{
    if (entryIndex < 0 || entryIndex >= kNumEntries || segment < 0)
        return {};
    const auto segs = splitSegments (kEntries[entryIndex].extra);
    return segment < (int) segs.size() ? segs[(size_t) segment] : std::string();
}

std::vector<Range> Searcher::highlightExtra (int entryIndex, int segment, const std::string& query) const
{
    std::vector<Range> r;
    if (entryIndex < 0 || entryIndex >= kNumEntries || segment < 0)
        return r;

    const auto& c = cache[(size_t) entryIndex];
    if (segment >= (int) c.lextra.size())
        return r;

    for (const auto& tok : tokenise (query))
    {
        bool ws = false;
        if (const int p = bestFind (c.lextra[(size_t) segment], tok, nullptr, ws); p >= 0)
            r.push_back ({ p, (int) tok.size() });
    }
    return mergeRanges (std::move (r));
}

} // namespace mc909search
