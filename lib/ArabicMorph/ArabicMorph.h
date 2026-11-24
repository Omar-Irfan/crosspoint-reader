#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Splits an Arabic word into prefixes, stem and suffixes and finds its root,
// using the Buckwalter Arabic Morphological Analyzer 1.0 data (GPL v2,
// converted by scripts/build_arabic_morph.py).
//
// Data file (UTF-8, Arabic script), lines:
//   #ARABIC_MORPH 2
//   P <tab> key <tab> parts <tab> category      prefix, parts joined by '+'
//   S <tab> key <tab> parts <tab> category      suffix
//   AB|AC|BC <tab> category <tab> category      allowed prefix-stem, prefix-suffix, stem-suffix pairs
//   I <tab> key <tab> offset                    first key of a stem block and its byte offset after #STEMS
//   #STEMS
//   key <tab> "vocalized category root" ...    sorted by key bytes, in blocks of at most MAX_BLOCK_BYTES
// Keys are written without vowel marks. The header (everything before
// #STEMS) is kept in memory; one stem lookup reads one block.
namespace ArabicMorph {

constexpr uint32_t MAX_BLOCK_BYTES = 4096;
constexpr uint32_t MAX_HEADER_BYTES = 256 * 1024;

struct Analysis {
  std::string breakdown;  // "وَ + بِ + كِتاب + هم"
  std::string root;       // "ك ت ب", empty when unknown
  std::string stem;       // the vocalized stem alone, "كِتاب"
};

// Reads up to len bytes at offset; returns the count read, or -1.
using ReadAt = std::function<int(uint32_t offset, char* buf, int len)>;

class Analyzer {
 public:
  Analyzer() = default;
  Analyzer(const Analyzer&) = delete;  // the tables are views into header
  Analyzer& operator=(const Analyzer&) = delete;

  // Loads the header of a file of fileSize bytes. False (and nothing kept)
  // when it is not a version 2 table or its header exceeds MAX_HEADER_BYTES.
  bool open(uint32_t fileSize, const ReadAt& read);
  bool isOpen() const { return !blocks.empty(); }
  uint32_t loadedFileSize() const { return fileSize; }

  // Analyses of one word (vowel marks are ignored), at most one per way of
  // splitting it into prefix, stem and suffix, longer stems first. read
  // must read the same file open() loaded. Empty when none is found.
  void analyze(std::string_view word, const ReadAt& read, std::vector<Analysis>& out, size_t maxResults) const;

 private:
  struct Affix {
    std::string_view key;
    std::string_view parts;
    std::string_view category;
  };
  struct Pair {
    std::string_view a;
    std::string_view b;
  };
  struct Block {
    std::string_view firstKey;
    uint32_t offset;  // from stemsStart
  };
  using AffixIt = std::vector<Affix>::const_iterator;

  // The stem's line ("key<tab>entries"); reads its block into block unless
  // blockIndex says that block is already there.
  bool findStemLine(std::string_view key, const ReadAt& read, std::string& block, size_t& blockIndex,
                    std::string_view& line) const;
  std::optional<Analysis> firstAllowed(std::string_view stemLine, AffixIt p0, AffixIt p1, AffixIt s0, AffixIt s1) const;
  static Analysis describe(std::string_view prefix, std::string_view stem, std::string_view suffix,
                           std::string_view root);
  static bool byKey(const Affix& x, const Affix& y) { return x.key < y.key; }
  static bool allowed(const std::vector<Pair>& table, std::string_view a, std::string_view b);
  void clear();

  std::string header;  // owns every view below
  std::vector<Affix> prefixes, suffixes;
  std::vector<Pair> tableAB, tableAC, tableBC;
  std::vector<Block> blocks;
  uint32_t stemsStart = 0;
  uint32_t fileSize = 0;
};

// The word without vowel marks or tatweel, with alef wasla written as alef.
std::string normalize(std::string_view word);

}  // namespace ArabicMorph
