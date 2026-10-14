#!/usr/bin/env python3
"""Generate Source/SearchData.cpp from Roland_MC-909_Patch_List.pdf.

The editor's search overlay is driven by a static table of everything printed
in the MC-909 patch list appendix: waveforms, preset patches (A-G), preset
rhythm sets, preset patterns, pattern sets, RPS patterns and RPS sets.

Requires `pdftotext` (poppler-utils). Run from anywhere:

    python3 tools/gen_search_data.py                # PDF + output next to this repo
    python3 tools/gen_search_data.py --json out.json  # also dump the parsed data

The parser reads the PDF's text layer in layout mode and relies on the
printed column structure, so it checks its own counts and exits non-zero if the
document does not parse to the expected sizes.
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# Expected sizes. If the PDF ever changes shape these trip loudly instead of
# silently producing a smaller table.
# ---------------------------------------------------------------------------
EXPECTED = {
    "waveforms": 693,
    "patches": {"A": 128, "B": 128, "C": 128, "D": 128, "E": 128, "F": 128, "G": 32},
    "kits": 72,
    "patterns": 215,
    "rps_patterns": 440,
    "rps_sets": 30,
    "pattern_sets": 12,
}

BANK_LSB = {"A": 64, "B": 65, "C": 66, "D": 67, "E": 68, "F": 69, "G": 70}   # CC#32, with CC#0 = 81
KIT_MSB, KIT_LSB = 82, 64                                                    # preset rhythm sets

# Must match enum class Kind in Source/SearchIndex.h
KIND = {"waveform": 0, "patch": 1, "rhythmKit": 2, "pattern": 3,
        "patternSet": 4, "rpsPattern": 5, "rpsSet": 6}


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------
JUNK = re.compile(r"^\s*(MC-909_Ref_e\.book|Appendices\s*$|\d{1,3}\s*$)")


def split_cols(line):
    return [t for t in re.split(r"\s{2,}", line.strip()) if t]


def read_pdf_lines(pdf):
    text = subprocess.run(["pdftotext", "-layout", str(pdf), "-"],
                          check=True, capture_output=True, text=True).stdout
    return [l for l in text.splitlines() if not JUNK.match(l)]


SECTIONS = [
    ("waveforms", "Waveform List"),
    ("patches", "Preset Patch List"),
    ("kits", "Preset Rhythm Set List"),
    ("patterns", "Preset Pattern List"),
    ("rps_patterns", "RPS Pattern List"),
    ("rps_sets", "RPS Set List"),
    ("pattern_sets", "Pattern Set List"),
]


def split_sections(lines):
    """Group lines under the section title that precedes them. A page-title
    line is the title text alone, possibly indented."""
    out = {k: [] for k, _ in SECTIONS}
    current = None
    page = {k: -1 for k, _ in SECTIONS}
    for l in lines:
        s = l.strip()
        hit = next((k for k, title in SECTIONS if s == title), None)
        if hit:
            current = hit
            page[hit] += 1
            out[hit].append(("PAGE", page[hit]))
        elif current:
            out[current].append(l)
    return out


def parse_waveforms(sec):
    out = {}
    for l in sec:
        if isinstance(l, tuple):
            continue
        c = split_cols(l)
        i = 0
        while i + 1 < len(c):
            if re.fullmatch(r"\d{3}", c[i]) and not re.fullmatch(r"\d{3}", c[i + 1]):
                out[int(c[i])] = c[i + 1]
                i += 2
            else:
                i += 1
    return out


def parse_patches(sec):
    # Each bank is 128 patches printed in two columns (001-064, 065-128), and
    # the column -> bank mapping is fixed per printed page. Bank G has only 32.
    page_cols = [["A", "A", "B", "B", "C"], ["C", "D", "D", "E", "E"], ["F", "F", "G"]]
    out = {b: {} for b in BANK_LSB}
    page = -1
    for l in sec:
        if isinstance(l, tuple):
            page = l[1]
            continue
        if page >= len(page_cols):
            continue
        c = split_cols(l)
        pairs, i = [], 0
        while i + 1 < len(c):
            if re.fullmatch(r"\d{3}", c[i]) and not re.fullmatch(r"\d{3}", c[i + 1]):
                pairs.append((int(c[i]), c[i + 1]))
                i += 2
            else:
                i += 1
        if not pairs or len(pairs) > len(page_cols[page]):
            continue
        for (n, name), bank in zip(pairs, page_cols[page]):
            out[bank][n] = name
    return out


def parse_kits(sec):
    """Rhythm sets are laid out as blocks of up to six side-by-side kits, each
    block introduced by a row of "NNN: name" headers. Cells are cut by the
    character column of each header because the sample-data sets (065-072) have
    blank cells that whitespace splitting cannot align."""
    head = re.compile(r"(\d{3}):\s")
    kits, pending = {}, None

    def flush():
        nonlocal pending
        if pending:
            for k in pending[1]:
                kits[k["number"]] = k
        pending = None

    for l in sec:
        if isinstance(l, tuple):
            continue
        hs = list(head.finditer(l))
        if hs:
            flush()
            starts = [m.start() for m in hs]
            names = []
            for j, m in enumerate(hs):
                end = starts[j + 1] if j + 1 < len(hs) else len(l)
                names.append(l[m.end():end].strip())
            pending = (starts, [{"number": int(m.group(1)), "name": nm, "cells": []}
                                for m, nm in zip(hs, names)])
            continue
        if pending is None:
            continue
        s = l.strip()
        if not s or s.startswith("*") or s.startswith("(Preset"):
            continue
        starts = pending[0]
        for j, k in enumerate(pending[1]):
            a = max(0, starts[j] - 1)
            b = starts[j + 1] - 1 if j + 1 < len(starts) else len(l)
            cell = l[a:b].strip()
            if cell:
                k["cells"].append(cell)
    flush()
    return [kits[n] for n in sorted(kits)]


def parse_patterns(sec):
    strict = re.compile(r"(\d{3})\s{2,}(.+?)\s{2,}(\d{2,3})\s+(\d{1,2})\s{2,}(.+?)(?=\s{2,}\d{3}\s{2,}|\s*$)")
    out = {}
    body = [l for l in sec if not isinstance(l, tuple)]
    for l in body:
        for m in strict.finditer(l):
            out[int(m.group(1))] = {"name": m.group(2).strip(), "bpm": int(m.group(3)),
                                    "measures": int(m.group(4)), "programmer": m.group(5).strip()}
    # Long names ("Reggae 1 (Dance Hall)") leave one space before the BPM column.
    # Only tried for numbers the strict pass missed, so it can't mis-split a clean row.
    for n in range(1, 216):
        if n in out:
            continue
        loose = re.compile(r"%03d\s{2,}(.+?)\s+(\d{2,3})\s+(\d{1,2})\s{2,}(.+?)(?=\s{2,}\d{3}\s{2,}|\s*$)" % n)
        for l in body:
            m = loose.search(l)
            if m:
                out[n] = {"name": m.group(1).strip(), "bpm": int(m.group(2)),
                          "measures": int(m.group(3)), "programmer": m.group(4).strip()}
                break
    return out


def parse_rps_patterns(sec):
    pat = re.compile(r"(\d{3})\s{2,}(.+?)(?=\s{2,}\d{3}\s{2,}|\s*$)")
    out = {}
    for l in sec:
        if isinstance(l, tuple) or l.strip().startswith("No."):
            continue
        for m in pat.finditer(l):
            out[int(m.group(1))] = m.group(2).strip()
    return out


def parse_sets(sec, row_re):
    set_head = re.compile(r"(\d{2})\.\s+(.+?)(?=\s{2,}\d{2}\.\s|\s*$)")
    out, group = {}, None
    for l in sec:
        if isinstance(l, tuple):
            continue
        heads = list(set_head.finditer(l))
        if heads and not row_re.search(l):
            group = []
            for m in heads:
                s = {"number": int(m.group(1)), "name": m.group(2).strip(), "pads": []}
                out[s["number"]] = s
                group.append(s)
            continue
        if not group:
            continue
        for s, m in zip(group, row_re.finditer(l)):
            s["pads"].append({"pad": int(m.group(1)), "pattern": int(m.group(2)),
                              "name": m.group(3).strip()})
    return [out[n] for n in sorted(out)]


def canonicalise_set_names(sets, names_by_number, what):
    """Set tables repeat each pad's pattern name, but the printed text is the
    least reliable part of the layout: the right-hand margin footnotes run into
    it, and Roland's own spelling varies ("Reggae 1(Dance Hall)", "scratch 12").
    The pattern *number* is exact, so names come from the pattern lists."""
    for s in sets:
        for p in s["pads"]:
            if p["pattern"] not in names_by_number:
                raise SystemExit(f"{what} {s['number']} pad {p['pad']} refers to unknown pattern {p['pattern']}")
            p["name"] = names_by_number[p["pattern"]]


def parse_all(pdf):
    sec = split_sections(read_pdf_lines(pdf))
    rps_row = re.compile(r"(\d{1,2})\s{2,}(\d{3})\s{2,}(.+?)(?=\s{2,}\d{1,2}\s{2,}\d{3}\s{2,}|\s*$)")
    pset_row = re.compile(r"(\d{1,2})\s{2,}(\d{1,3})\s{2,}(.+?)(?=\s{2,}\d{1,2}\s{2,}\d{1,3}\s{2,}|\s*$)")
    d = {
        "waveforms": parse_waveforms(sec["waveforms"]),
        "patches": parse_patches(sec["patches"]),
        "kits": parse_kits(sec["kits"]),
        "patterns": parse_patterns(sec["patterns"]),
        "rps_patterns": parse_rps_patterns(sec["rps_patterns"]),
        "rps_sets": parse_sets(sec["rps_sets"], rps_row),
        "pattern_sets": parse_sets(sec["pattern_sets"], pset_row),
    }
    canonicalise_set_names(d["rps_sets"], d["rps_patterns"], "RPS set")
    canonicalise_set_names(d["pattern_sets"], {n: p["name"] for n, p in d["patterns"].items()}, "Pattern set")
    return d


def check_counts(d):
    got = {
        "waveforms": len(d["waveforms"]),
        "patches": {b: len(v) for b, v in d["patches"].items()},
        "kits": len(d["kits"]),
        "patterns": len(d["patterns"]),
        "rps_patterns": len(d["rps_patterns"]),
        "rps_sets": len(d["rps_sets"]),
        "pattern_sets": len(d["pattern_sets"]),
    }
    ok = got == EXPECTED
    for s in d["rps_sets"] + d["pattern_sets"]:
        ok &= len(s["pads"]) == 16
    waves = set(d["waveforms"].values())
    for k in d["kits"]:
        # 001-064 are full 16-pad kits made of internal waveforms; 065-072 are
        # short sample-data sets whose names are not in the waveform list.
        if k["number"] <= 64:
            ok &= len(k["cells"]) == 16 and all(c in waves for c in k["cells"])
        else:
            ok &= 1 <= len(k["cells"]) <= 16
    return ok, got


# ---------------------------------------------------------------------------
# Entry table
# ---------------------------------------------------------------------------
NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def note_name(n):                       # C4 = 60, as the MC-909 labels its keys
    return f"{NOTE_NAMES[n % 12]}{n // 12 - 1}"


def build_entries(d):
    """-> list of (kind, name, where, extra, detail, number, bank)."""
    entries = []
    wave_no = {}                        # waveform name -> number (first wins)
    for n, name in d["waveforms"].items():
        wave_no.setdefault(name, n)

    # Which preset kits use each waveform (matched by name).
    used_in = {}
    for k in d["kits"]:
        for i, cell in enumerate(k["cells"]):
            if cell in wave_no:
                used_in.setdefault(wave_no[cell], []).append((k, 59 + i))

    # Which RPS sets use each RPS pattern.
    rps_used = {}
    for s in d["rps_sets"]:
        for p in s["pads"]:
            rps_used.setdefault(p["pattern"], []).append((s, p["pad"]))

    for n in sorted(d["waveforms"]):
        name = d["waveforms"][n]
        lines = ["Wave group\tINT (internal)", f"Wave number\t{n}"]
        uses = used_in.get(n, [])
        if uses:
            kits_seen = []
            for k, note in uses:
                if k["number"] not in [x[0]["number"] for x in kits_seen]:
                    kits_seen.append((k, note))
            lines.append(f"# Used in {len(kits_seen)} preset rhythm set{'s' if len(kits_seen) != 1 else ''}")
            for k, note in kits_seen[:12]:
                lines.append(f"Kit {k['number']:03d}\t{k['name']}  ({note_name(note)})")
            if len(kits_seen) > 12:
                lines.append(f"\t... and {len(kits_seen) - 12} more")
        entries.append(("waveform", name, f"Waveform {n:03d}", "", "\n".join(lines), n, 0))

    for bi, bank in enumerate(sorted(BANK_LSB)):
        for n in sorted(d["patches"][bank]):
            lines = ["Bank MSB\t81  (CC#0)", f"Bank LSB\t{BANK_LSB[bank]}  (CC#32)",
                     f"Program\t{n}  (PC {n - 1})"]
            entries.append(("patch", d["patches"][bank][n], f"Preset {bank} {n:03d}", "",
                            "\n".join(lines), n, bi))

    for k in d["kits"]:
        n = k["number"]
        lines = [f"Bank MSB\t{KIT_MSB}  (CC#0)", f"Bank LSB\t{KIT_LSB}  (CC#32)",
                 f"Program\t{n}  (PC {n - 1})"]
        if n > 64:
            lines.append("# Sample set (used by factory patterns)")
        else:
            lines.append("# Keys")
        for i, cell in enumerate(k["cells"]):
            lines.append(f"{note_name(59 + i)}  {59 + i}\t{cell}")
        entries.append(("rhythmKit", k["name"], f"Preset Rhythm {n:03d}", "|".join(k["cells"]),
                        "\n".join(lines), n, 0))

    for n in sorted(d["patterns"]):
        p = d["patterns"][n]
        lines = [f"Tempo\t{p['bpm']} BPM", f"Length\t{p['measures']} measures",
                 f"Programmer\t{p['programmer']}"]
        extra = "|".join([f"{p['bpm']} BPM", f"{p['measures']} measures", p["programmer"]])
        entries.append(("pattern", p["name"], f"Preset Pattern {n:03d}", extra, "\n".join(lines), n, 0))

    for s in d["pattern_sets"]:
        lines = ["# Patterns"] + [f"Pad {p['pad']}\t#{p['pattern']}  {p['name']}" for p in s["pads"]]
        entries.append(("patternSet", s["name"], f"Pattern Set {s['number']:02d}",
                        "|".join(p["name"] for p in s["pads"]), "\n".join(lines), s["number"], 0))

    for n in sorted(d["rps_patterns"]):
        lines = []
        uses = rps_used.get(n, [])
        if uses:
            lines.append(f"# Used in {len(uses)} RPS set{'s' if len(uses) != 1 else ''}")
            for s, pad in uses[:12]:
                lines.append(f"Set {s['number']:02d}\t{s['name']}  (pad {pad})")
            if len(uses) > 12:
                lines.append(f"\t... and {len(uses) - 12} more")
        else:
            lines.append("Not assigned to an RPS set")
        entries.append(("rpsPattern", d["rps_patterns"][n], f"RPS Pattern {n:03d}", "",
                        "\n".join(lines), n, 0))

    for s in d["rps_sets"]:
        lines = ["# Pads"] + [f"Pad {p['pad']}\t#{p['pattern']}  {p['name']}" for p in s["pads"]]
        entries.append(("rpsSet", s["name"], f"RPS Set {s['number']:02d}",
                        "|".join(p["name"] for p in s["pads"]), "\n".join(lines), s["number"], 0))
    return entries


# ---------------------------------------------------------------------------
# C++ emission
# ---------------------------------------------------------------------------
def cstr(s):
    """A C++ string literal. The source text is plain ASCII; anything else is
    escaped as UTF-8 bytes so the file never depends on the compiler's charset."""
    out = ['"']
    for ch in s:
        o = ord(ch)
        if ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif ch == "?":
            out.append("\\?")           # never form a trigraph
        elif 32 <= o < 127:
            out.append(ch)
        else:
            out.extend("\\x%02x" % b for b in ch.encode("utf-8"))
            out.append('""')            # stop the hex escape swallowing the next char
    out.append('"')
    return "".join(out)


def emit_cpp(entries, path):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("// AUTO-GENERATED by tools/gen_search_data.py from Roland_MC-909_Patch_List.pdf.\n")
        f.write("// Do not edit by hand: re-run the script instead.\n")
        f.write('#include "SearchIndex.h"\n\n')
        f.write("namespace mc909search\n{\n\n")
        f.write("const Entry kEntries[] = {\n")
        for kind, name, where, extra, detail, number, bank in entries:
            f.write(f"    {{ Kind({KIND[kind]}), {cstr(name)}, {cstr(where)}, {cstr(extra)},\n"
                    f"      {cstr(detail)}, {number}, {bank} }},\n")
        f.write("};\n\n")
        f.write("const int kNumEntries = (int) (sizeof (kEntries) / sizeof (kEntries[0]));\n\n")
        f.write("} // namespace mc909search\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pdf", default=str(ROOT / "Roland_MC-909_Patch_List.pdf"))
    ap.add_argument("--out", default=str(ROOT / "Source" / "SearchData.cpp"))
    ap.add_argument("--json", help="also write the parsed data as JSON")
    args = ap.parse_args()

    data = parse_all(args.pdf)
    ok, got = check_counts(data)
    print("parsed:", json.dumps(got), file=sys.stderr)
    if not ok:
        print("ERROR: parsed sizes do not match the expected layout; not writing output.\n"
              f"expected: {json.dumps(EXPECTED)}", file=sys.stderr)
        return 1

    if args.json:
        Path(args.json).write_text(json.dumps(data, indent=1, ensure_ascii=False), encoding="utf-8")

    entries = build_entries(data)
    emit_cpp(entries, args.out)
    kinds = {}
    for e in entries:
        kinds[e[0]] = kinds.get(e[0], 0) + 1
    print(f"wrote {len(entries)} entries to {args.out}  {kinds}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
