#!/usr/bin/env python3
"""Builds the word-breakdown table for Arabic dictionary lookups.

Input: the six data files of the Buckwalter Arabic Morphological Analyzer 1.0
(dictPrefixes, dictSuffixes, dictStems, tableAB, tableAC, tableBC; GPL v2,
(c) 2002 QAMUS LLC and the Trustees of the University of Pennsylvania).
Output: one UTF-8 text file in Arabic script, copied to the SD card as
/.crosspoint/arabic_morph.txt (format: lib/ArabicMorph/ArabicMorph.h).

Usage: build_arabic_morph.py <buckwalter data dir> <output file>
"""
import os
import sys

BW = {
    "'": "ء", "|": "آ", ">": "أ", "&": "ؤ", "<": "إ", "}": "ئ", "A": "ا",
    "b": "ب", "p": "ة", "t": "ت", "v": "ث", "j": "ج", "H": "ح", "x": "خ",
    "d": "د", "*": "ذ", "r": "ر", "z": "ز", "s": "س", "$": "ش", "S": "ص",
    "D": "ض", "T": "ط", "Z": "ظ", "E": "ع", "g": "غ", "_": "", "f": "ف",
    "q": "ق", "k": "ك", "l": "ل", "m": "م", "n": "ن", "h": "ه", "w": "و",
    "Y": "ى", "y": "ي", "F": "ً", "N": "ٌ", "K": "ٍ", "a": "َ", "u": "ُ",
    "i": "ِ", "~": "ّ", "o": "", "`": "ٰ", "{": "ٱ",
}  # sukun (o) and tatweel (_) are dropped: they only clutter the breakdown


MARKS = set("FNKauio~`")


def strip_marks(bw):
    """Lookup keys are written without vowel marks (a few source keys have them)."""
    return "".join(c for c in bw if c not in MARKS)


def arabic(bw):
    return "".join(BW.get(c, "") for c in bw)


def data_lines(path):
    with open(path, encoding="latin-1") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if line and not line.startswith(";"):
                yield line


def segments(voc, gloss):
    """The written affix split into its parts: 'wabi' with <pos>wa/CONJ+bi/PREP+</pos> -> 'وَ+بِ'.
    When the parts do not spell the written form (a part merged into the stem), it stays whole."""
    parts = []
    if "<pos>" in gloss:
        body = gloss.split("<pos>", 1)[1].split("</pos>", 1)[0]
        parts = [p.split("/", 1)[0] for p in body.split("+") if "/" in p and not p.startswith("(null)")]
    if "".join(parts) != voc:
        parts = [voc]
    return "+".join(s for s in (arabic(p) for p in parts) if s)


def affixes(path, tag):
    out = set()
    for line in data_lines(path):
        f = line.split("\t")
        if len(f) < 3:
            continue
        out.add(f"{tag}\t{arabic(f[0])}\t{segments(f[1], f[3] if len(f) > 3 else '')}\t{f[2]}")
    return sorted(out)


def table(path, tag):
    return sorted({f"{tag}\t{a}\t{b}" for a, b in (l.split() for l in data_lines(path) if len(l.split()) == 2)})


FOLD = str.maketrans({"|": "A", ">": "A", "<": "A", "&": "A", "}": "A", "'": "A", "{": "A", "Y": "y", "p": "t"})


def root_fits(root, key):
    """Some stems sit under the wrong root heading; a root whose strong letters
    are not all in the stem, in order, is dropped rather than shown wrong."""
    letters = [c for c in root.translate(FOLD) if c not in "Awy"]
    it = iter(key.translate(FOLD))
    return all(c in it for c in letters)


def stems(path):
    by_key, root = {}, ""
    with open(path, encoding="latin-1") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if line.startswith(";--- "):
                root = line[5:].split()[0] if len(line) > 5 else ""
                continue
            if not line or line.startswith(";"):
                continue
            f4 = line.split("\t")
            if len(f4) < 3:
                continue
            key, voc, cat = arabic(strip_marks(f4[0])), arabic(f4[1]), f4[2]
            entry = f"{voc} {cat} {arabic(root) if root and root_fits(root, f4[0]) else '-'}"
            entries = by_key.setdefault(key, [])
            if entry not in entries:
                entries.append(entry)
    return [f"{k}\t" + "\t".join(v) for k, v in sorted(by_key.items(), key=lambda kv: kv[0].encode())]


MAX_BLOCK_BYTES = 4096  # lib/ArabicMorph/ArabicMorph.h


def blocks(stem_lines):
    """Groups the sorted stem lines into blocks of at most MAX_BLOCK_BYTES;
    returns (first key, byte offset) per block."""
    index, offset, size = [], 0, 0
    for line in stem_lines:
        n = len(line.encode()) + 1
        if n > MAX_BLOCK_BYTES:
            sys.exit(f"stem line longer than {MAX_BLOCK_BYTES} bytes: {line[:40]}")
        if not index or size + n > MAX_BLOCK_BYTES:
            index.append((line.split("\t", 1)[0], offset))
            size = 0
        size += n
        offset += n
    return index


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, out = sys.argv[1], sys.argv[2]
    stem_lines = stems(os.path.join(src, "dictStems"))
    lines = ["#ARABIC_MORPH 2"]
    lines += affixes(os.path.join(src, "dictPrefixes"), "P")
    lines += affixes(os.path.join(src, "dictSuffixes"), "S")
    for name in ("AB", "AC", "BC"):
        lines += table(os.path.join(src, "table" + name), name)
    lines += [f"I\t{key}\t{offset}" for key, offset in blocks(stem_lines)]
    lines.append("#STEMS")
    lines += stem_lines
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"wrote {out}: {len(lines)} lines, {os.path.getsize(out) / 1024 / 1024:.1f} MB")


if __name__ == "__main__":
    main()
