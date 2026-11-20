#include "ArabicMorph.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <tuple>

namespace ArabicMorph {
namespace {

// Every Arabic letter and mark is one 2-byte UTF-8 sequence (U+0600..U+06FF),
// so affixes are split at even byte offsets.
constexpr size_t CHAR_BYTES = 2;
constexpr size_t MAX_PREFIX_CHARS = 4;
constexpr size_t MAX_SUFFIX_CHARS = 6;
constexpr std::string_view HEADER = "#ARABIC_MORPH 2\n";
constexpr std::string_view STEMS_MARKER = "#STEMS\n";

bool isArabicByte(const unsigned char lead) { return lead >= 0xD8 && lead <= 0xDB; }

uint32_t codepointAt(std::string_view s, const size_t i) {
  return ((static_cast<unsigned char>(s[i]) & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
}

bool isMark(const uint32_t cp) { return (cp >= 0x064B && cp <= 0x0652) || cp == 0x0670; }

// Splits s at sep into at most n fields; false when fewer are present.
bool splitFields(std::string_view s, const char sep, std::string_view* fields, const size_t n) {
  for (size_t i = 0; i < n; ++i) {
    const size_t end = i + 1 == n ? s.size() : s.find(sep);
    if (end == std::string_view::npos) return false;
    fields[i] = s.substr(0, end);
    s.remove_prefix(i + 1 == n ? s.size() : end + 1);
  }
  return true;
}

// "وَ+بِ" -> appends "وَ" and "بِ" to parts.
void appendParts(std::string_view joined, std::vector<std::string_view>& parts) {
  while (!joined.empty()) {
    const size_t plus = joined.find('+');
    parts.push_back(joined.substr(0, plus));
    if (plus == std::string_view::npos) break;
    joined.remove_prefix(plus + 1);
  }
}

}  // namespace

std::string normalize(std::string_view word) {
  std::string out;
  out.reserve(word.size());
  for (size_t i = 0; i < word.size(); ++i) {
    const auto c = static_cast<unsigned char>(word[i]);
    if (isArabicByte(c) && i + 1 < word.size()) {
      const uint32_t cp = codepointAt(word, i);
      if (cp == 0x0671) {
        out += "\xD8\xA7";  // ٱ -> ا
      } else if (!isMark(cp) && cp != 0x0640) {
        out.append(word.data() + i, CHAR_BYTES);
      }
      ++i;
    } else {
      out += static_cast<char>(c);
    }
  }
  return out;
}

void Analyzer::clear() {
  std::string{}.swap(header);
  for (auto* list : {&prefixes, &suffixes}) std::vector<Affix>{}.swap(*list);
  for (auto* table : {&tableAB, &tableAC, &tableBC}) std::vector<Pair>{}.swap(*table);
  std::vector<Block>{}.swap(blocks);
  stemsStart = 0;
  fileSize = 0;
}

bool Analyzer::open(const uint32_t size, const ReadAt& read) {
  clear();
  constexpr int CHUNK_BYTES = 4096;
  std::string text;
  // Read until the stems marker; a file that is not a table fails on its
  // first line, and the header is never read past MAX_HEADER_BYTES.
  for (uint32_t offset = 0; offset < size && offset < MAX_HEADER_BYTES;) {
    const size_t searchFrom = text.size() > STEMS_MARKER.size() ? text.size() - STEMS_MARKER.size() : 0;
    const int want = static_cast<int>(std::min<uint32_t>(CHUNK_BYTES, size - offset));
    if (text.empty()) text.reserve(MAX_HEADER_BYTES / 2);
    text.resize(text.size() + want);
    const int n = read(offset, text.data() + text.size() - want, want);
    if (n <= 0) return false;
    text.resize(text.size() - want + n);
    offset += n;
    if (text.size() >= HEADER.size() && text.compare(0, HEADER.size(), HEADER) != 0) return false;
    const size_t marker = text.find(STEMS_MARKER, searchFrom);
    if (marker != std::string::npos) {
      stemsStart = static_cast<uint32_t>(marker + STEMS_MARKER.size());
      text.resize(marker);
      break;
    }
  }
  if (stemsStart == 0 || text.compare(0, HEADER.size(), HEADER) != 0) {
    stemsStart = 0;
    return false;
  }
  text.shrink_to_fit();
  header = std::move(text);

  // Count first so each table is allocated once.
  size_t counts[6] = {};  // P, S, AB, AC, BC, I
  const auto kindOf = [](std::string_view line) -> int {
    if (line.starts_with("P\t")) return 0;
    if (line.starts_with("S\t")) return 1;
    if (line.starts_with("AB\t")) return 2;
    if (line.starts_with("AC\t")) return 3;
    if (line.starts_with("BC\t")) return 4;
    if (line.starts_with("I\t")) return 5;
    return -1;
  };
  const auto forEachLine = [&](const auto& fn) {
    std::string_view rest(header);
    rest.remove_prefix(HEADER.size());
    while (!rest.empty()) {
      const size_t end = rest.find('\n');
      fn(rest.substr(0, end));
      rest.remove_prefix(end == std::string_view::npos ? rest.size() : end + 1);
    }
  };
  forEachLine([&](std::string_view line) {
    if (const int kind = kindOf(line); kind >= 0) ++counts[kind];
  });
  prefixes.reserve(counts[0]);
  suffixes.reserve(counts[1]);
  tableAB.reserve(counts[2]);
  tableAC.reserve(counts[3]);
  tableBC.reserve(counts[4]);
  blocks.reserve(counts[5]);
  forEachLine([&](std::string_view line) {
    std::string_view f[4];
    switch (kindOf(line)) {
      case 0:
      case 1:
        if (splitFields(line, '\t', f, 4)) (line[0] == 'P' ? prefixes : suffixes).push_back({f[1], f[2], f[3]});
        break;
      case 2:
      case 3:
      case 4:
        if (splitFields(line, '\t', f, 3)) {
          (f[0] == "AB" ? tableAB : f[0] == "AC" ? tableAC : tableBC).push_back({f[1], f[2]});
        }
        break;
      case 5:
        if (splitFields(line, '\t', f, 3)) {
          blocks.push_back({f[1], static_cast<uint32_t>(std::strtoul(std::string(f[2]).c_str(), nullptr, 10))});
        }
        break;
      default:
        break;
    }
  });
  // The file is written sorted; sort again so a hand-edited file still works.
  std::stable_sort(prefixes.begin(), prefixes.end(), byKey);
  std::stable_sort(suffixes.begin(), suffixes.end(), byKey);
  for (auto* t : {&tableAB, &tableAC, &tableBC}) {
    std::sort(t->begin(), t->end(),
              [](const Pair& x, const Pair& y) { return std::tie(x.a, x.b) < std::tie(y.a, y.b); });
  }
  if (blocks.empty()) {
    clear();
    return false;
  }
  fileSize = size;
  return true;
}

bool Analyzer::allowed(const std::vector<Pair>& table, std::string_view a, std::string_view b) {
  return std::binary_search(table.begin(), table.end(), Pair{a, b},
                            [](const Pair& x, const Pair& y) { return std::tie(x.a, x.b) < std::tie(y.a, y.b); });
}

bool Analyzer::findStemLine(std::string_view key, const ReadAt& read, std::string& block, size_t& blockIndex,
                            std::string_view& line) const {
  // The last block whose first key is not after key.
  const auto after = std::upper_bound(blocks.begin(), blocks.end(), key,
                                      [](std::string_view k, const Block& b) { return k < b.firstKey; });
  if (after == blocks.begin()) return false;
  const size_t index = static_cast<size_t>(after - blocks.begin()) - 1;
  if (index != blockIndex) {
    const uint32_t start = stemsStart + blocks[index].offset;
    const uint32_t end = index + 1 < blocks.size() ? stemsStart + blocks[index + 1].offset : fileSize;
    if (end <= start || end - start > MAX_BLOCK_BYTES) return false;
    block.resize(end - start);
    if (read(start, block.data(), static_cast<int>(block.size())) != static_cast<int>(block.size())) {
      blockIndex = SIZE_MAX;
      return false;
    }
    blockIndex = index;
  }
  std::string_view rest(block);
  while (!rest.empty()) {
    const size_t end = rest.find('\n');
    const std::string_view candidate = rest.substr(0, end);
    if (candidate.substr(0, candidate.find('\t')) == key) {
      line = candidate;
      return true;
    }
    rest.remove_prefix(end == std::string_view::npos ? rest.size() : end + 1);
  }
  return false;
}

void Analyzer::analyze(std::string_view word, const ReadAt& read, std::vector<Analysis>& out,
                       const size_t maxResults) const {
  out.clear();
  if (!isOpen() || maxResults == 0) return;
  const std::string w = normalize(word);
  if (w.empty() || w.size() % CHAR_BYTES != 0) return;
  for (size_t i = 0; i < w.size(); i += CHAR_BYTES) {
    if (!isArabicByte(static_cast<unsigned char>(w[i]))) return;
  }
  const std::string_view sv(w);
  const size_t chars = w.size() / CHAR_BYTES;
  const auto range = [](const std::vector<Affix>& list, std::string_view key) {
    return std::equal_range(list.begin(), list.end(), Affix{key, {}, {}}, byKey);
  };

  // One analysis per way of splitting the word, longer stems first.
  std::string block;
  block.reserve(MAX_BLOCK_BYTES);
  size_t blockIndex = SIZE_MAX;
  for (size_t stemChars = chars; stemChars >= 1 && out.size() < maxResults; --stemChars) {
    for (size_t pre = 0; pre <= std::min(MAX_PREFIX_CHARS, chars - stemChars) && out.size() < maxResults; ++pre) {
      if (chars - stemChars - pre > MAX_SUFFIX_CHARS) continue;
      const auto [p0, p1] = range(prefixes, sv.substr(0, pre * CHAR_BYTES));
      const auto [s0, s1] = range(suffixes, sv.substr((pre + stemChars) * CHAR_BYTES));
      if (p0 == p1 || s0 == s1) continue;
      std::string_view stemLine;
      if (!findStemLine(sv.substr(pre * CHAR_BYTES, stemChars * CHAR_BYTES), read, block, blockIndex, stemLine)) {
        continue;
      }
      if (const auto found = firstAllowed(stemLine, p0, p1, s0, s1)) out.push_back(*found);
    }
  }
}

std::optional<Analysis> Analyzer::firstAllowed(std::string_view stemLine, AffixIt p0, AffixIt p1, AffixIt s0,
                                               AffixIt s1) const {
  std::string_view entries = stemLine.substr(stemLine.find('\t') + 1);
  while (!entries.empty()) {
    const size_t tab = entries.find('\t');
    std::string_view e[3];  // vocalized stem, category, root
    const bool parsed = splitFields(entries.substr(0, tab), ' ', e, 3);
    entries.remove_prefix(tab == std::string_view::npos ? entries.size() : tab + 1);
    if (!parsed) continue;
    for (auto p = p0; p != p1; ++p) {
      if (!allowed(tableAB, p->category, e[1])) continue;
      for (auto s = s0; s != s1; ++s) {
        if (allowed(tableAC, p->category, s->category) && allowed(tableBC, e[1], s->category)) {
          return describe(p->parts, e[0], s->parts, e[2]);
        }
      }
    }
  }
  return std::nullopt;
}

Analysis Analyzer::describe(std::string_view prefix, std::string_view stemText, std::string_view suffix,
                            std::string_view root) {
  std::vector<std::string_view> parts;
  appendParts(prefix, parts);
  std::string stem(stemText);
  // Marks written at the start of a suffix belong on the stem's last letter.
  while (suffix.size() >= CHAR_BYTES && isMark(codepointAt(suffix, 0))) {
    stem.append(suffix.substr(0, CHAR_BYTES));
    suffix.remove_prefix(CHAR_BYTES);
  }
  parts.push_back(stem);
  appendParts(suffix, parts);

  Analysis a;
  a.stem.assign(stemText);
  for (const auto part : parts) {
    if (part.empty()) continue;
    if (!a.breakdown.empty()) a.breakdown += " + ";
    a.breakdown.append(part);
  }
  if (root != "-") {
    for (size_t i = 0; i + 1 < root.size(); i += CHAR_BYTES) {
      if (!a.root.empty()) a.root += ' ';
      a.root.append(root.substr(i, CHAR_BYTES));
    }
  }
  return a;
}

}  // namespace ArabicMorph
