#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ArabicStem {

// Upper bound on fallback forms per lookup; keeps the probe count (and the
// lookup's working set) bounded on long clitic chains.
constexpr size_t MAX_VARIANTS = 24;
// Upper bound on candidates considered before ranking: the transient working
// set of variants() is at most this many word-sized strings.
constexpr size_t MAX_CANDIDATES = 64;

// True when the UTF-8 text contains an Arabic letter (U+0621..U+064A, U+0671..U+06D3).
bool containsArabic(std::string_view word);

// Arabic punctuation that dictionary lookups trim from word edges:
// ، ؛ ؟ ٪ ٫ ٬ ٭ ۔ and the ornate parentheses ﴾ ﴿.
inline bool isArabicPunctuation(const uint32_t cp) {
  return cp == 0x060C || cp == 0x061B || cp == 0x061F || (cp >= 0x066A && cp <= 0x066D) || cp == 0x06D4 ||
         cp == 0xFD3E || cp == 0xFD3F;
}

// Fills out with lookup fallbacks for an Arabic word whose vowel marks are
// already stripped: without shadda, hamza-on-alef folded, proclitics
// (و ف ب ك ل س, ال, لل) and pronoun enclitics (ـه ـها ـهم ...) removed, with
// the spelling a suffix changed restored (ت -> ة, ـو -> ـوا, ؤ/ئ -> ء).
// Ordered fewest changes first, never contains word itself, at most
// MAX_VARIANTS entries. out is cleared first.
void variants(const std::string& word, std::vector<std::string>& out);

}  // namespace ArabicStem
