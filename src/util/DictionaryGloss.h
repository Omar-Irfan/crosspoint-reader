#pragma once

#include <string>
#include <vector>

// Word-by-word gloss for a selected phrase that is not itself a dictionary
// entry: each word is looked up on its own and shown with a short snippet.
namespace DictionaryGloss {

// Most words glossed from one selection; bounds lookup time and the page size.
constexpr size_t MAX_WORDS = 16;
// Snippet length per word, in bytes (cut on a UTF-8 boundary).
constexpr size_t MAX_SNIPPET_BYTES = 160;

// Splits a selection on whitespace into at most MAX_WORDS words.
void splitWords(const std::string& text, std::vector<std::string>& out);

// First meaningful lines of a plain-text definition (htmlToPlainText output):
// skips blank lines, heading lines (starting with headingMarker), source
// labels ("[Wiktionary] ..."), pronunciation lines, inflection lines
// (containing a bullet •) and lines ending in ':'. Lines that only name a
// grammatical form of another word ("... jussive of ...") are used only when
// there is nothing else.
// At most two lines joined by "; ", both from the same source, cut to
// MAX_SNIPPET_BYTES. When source is
// given, it receives the label of the last "[Label] ..." line before the
// snippet (a merged dictionary's source name), or "" when there is none.
// preferStem (a vocalized stem from the word breakdown) picks, in an entry
// that merges several words spelled alike, the "[Label] headword" section
// whose headword it matches; without a match the whole entry is used.
std::string snippet(const std::string& plain, char headingMarker, std::string* source = nullptr,
                    const std::string& preferStem = {});

}  // namespace DictionaryGloss
