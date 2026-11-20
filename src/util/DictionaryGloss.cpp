#include "DictionaryGloss.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace DictionaryGloss {
namespace {

bool isSpace(const char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

bool startsWith(const std::string& s, const char* p) { return s.compare(0, strlen(p), p) == 0; }

bool isNoiseLine(const std::string& line, const char headingMarker) {
  if (line.empty() || line[0] == headingMarker || line[0] == '[') return true;
  if (startsWith(line, "IPA") || line.find("IPA:") != std::string::npos) return true;
  // Wiktionary headword lines list inflected forms separated by a bullet.
  if (line.find("\xE2\x80\xA2") != std::string::npos) return true;
  return line.back() == ':';
}

// A sense that only names a grammatical form of another word
// ("first-person singular non-past active jussive of هَالَ"); the wording
// is English Wiktionary's.
bool isFormOfLine(const std::string& line) {
  if (line.find(" of ") == std::string::npos) return false;
  for (const char* marker : {"-person", "jussive", "subjunctive", "indicative", "imperative", "inflection of",
                             "form of", "plural of", "dual of", "feminine of", "singular of"}) {
    if (line.find(marker) != std::string::npos) return true;
  }
  return false;
}

std::string trim(const std::string& s) {
  size_t b = 0;
  size_t e = s.size();
  while (b < e && isSpace(s[b])) ++b;
  while (e > b && isSpace(s[e - 1])) --e;
  return s.substr(b, e - b);
}

// Cut to at most maxBytes without splitting a UTF-8 sequence.
void cutUtf8(std::string& s, const size_t maxBytes) {
  if (s.size() <= maxBytes) return;
  size_t n = maxBytes;
  while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
  s.resize(n);
  s += "…";
}

// Dictionary headwords (كِتَاب, قَالَ, اِسْتَقَالَ) and the word breakdown's
// stems (كِتاب, قال, ٱِستَقال) mark vowels differently; this keeps what both
// write the same way: no sukun, no fatha before alef, alef wasla as alef, no
// marks on a word-initial alef and no final short vowel or tanween.
std::string foldHeadword(const std::string& word) {
  constexpr uint32_t ALEF = 0x0627, ALEF_WASLA = 0x0671, FATHA = 0x064E, SUKUN = 0x0652;
  const auto isMark = [](const uint32_t cp) { return cp >= 0x064B && cp <= 0x0652; };
  std::vector<uint32_t> cps;
  for (size_t i = 0; i < word.size();) {
    const auto lead = static_cast<unsigned char>(word[i]);
    if (lead >= 0xD8 && lead <= 0xDB && i + 1 < word.size()) {
      cps.push_back(((lead & 0x1F) << 6) | (static_cast<unsigned char>(word[i + 1]) & 0x3F));
      i += 2;
    } else {
      cps.push_back(lead);
      ++i;
    }
  }
  std::vector<uint32_t> kept;
  for (size_t i = 0; i < cps.size(); ++i) {
    const uint32_t cp = cps[i] == ALEF_WASLA ? ALEF : cps[i];
    if (cp == SUKUN) continue;
    if (cp == FATHA && i + 1 < cps.size() && (cps[i + 1] == ALEF || cps[i + 1] == ALEF_WASLA)) continue;
    if (isMark(cp) && kept.size() == 1 && kept[0] == ALEF) continue;
    kept.push_back(cp);
  }
  while (!kept.empty() && isMark(kept.back()) && kept.back() != 0x0651) kept.pop_back();  // not shadda
  std::string out;
  for (const uint32_t cp : kept) {
    if (cp < 0x80) {
      out += static_cast<char>(cp);
    } else {
      out += static_cast<char>(0xC0 | (cp >> 6));
      out += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }
  return out;
}

// Byte offset of the "[Label] headword" line whose headword matches stem, or npos.
size_t sectionFor(const std::string& plain, const std::string& stem) {
  const std::string want = foldHeadword(stem);
  for (size_t pos = 0; pos < plain.size();) {
    size_t end = plain.find('\n', pos);
    if (end == std::string::npos) end = plain.size();
    const std::string line = trim(plain.substr(pos, end - pos));
    const size_t close = line.find("] ");
    if (!line.empty() && line[0] == '[' && close != std::string::npos &&
        foldHeadword(trim(line.substr(close + 2))) == want) {
      return pos;
    }
    pos = end + 1;
  }
  return std::string::npos;
}

// The first senses of plain; with oneSection, only of its first
// "[Label] headword" section.
std::string firstSenses(const std::string& plain, const char headingMarker, std::string* source,
                        const bool oneSection) {
  // A merged entry lists every word spelled the same way; prefer real senses
  // over lines naming a grammatical form of some other word.
  for (const bool skipFormOf : {true, false}) {
    std::string out;
    std::string label;
    int kept = 0;
    int labels = 0;
    size_t pos = 0;
    while (pos < plain.size() && kept < 2) {
      size_t end = plain.find('\n', pos);
      if (end == std::string::npos) end = plain.size();
      const std::string line = trim(plain.substr(pos, end - pos));
      pos = end + 1;
      if (!line.empty() && line[0] == '[') {
        ++labels;
        if (kept > 0 || (oneSection && labels > 1)) break;  // another section's lines: keep one label
      }
      if (kept == 0 && !line.empty() && line[0] == '[') {
        const size_t close = line.find(']');
        if (close != std::string::npos) label = line.substr(1, close - 1);
      }
      if (isNoiseLine(line, headingMarker) || (skipFormOf && isFormOfLine(line))) continue;
      if (kept > 0) out += "; ";
      out += line;
      ++kept;
    }
    if (kept == 0 && skipFormOf) continue;
    cutUtf8(out, MAX_SNIPPET_BYTES);
    if (source) *source = kept > 0 ? label : std::string{};
    return out;
  }
  if (source) source->clear();
  return {};
}

}  // namespace

void splitWords(const std::string& text, std::vector<std::string>& out) {
  out.clear();
  size_t i = 0;
  while (i < text.size() && out.size() < MAX_WORDS) {
    while (i < text.size() && isSpace(text[i])) ++i;
    const size_t start = i;
    while (i < text.size() && !isSpace(text[i])) ++i;
    if (i > start) out.emplace_back(text, start, i - start);
  }
}

std::string snippet(const std::string& plain, const char headingMarker, std::string* source,
                    const std::string& preferStem) {
  if (!preferStem.empty()) {
    if (const size_t at = sectionFor(plain, preferStem); at != std::string::npos) {
      std::string out = firstSenses(plain.substr(at), headingMarker, source, true);
      if (!out.empty()) return out;
    }
  }
  return firstSenses(plain, headingMarker, source, false);
}

}  // namespace DictionaryGloss
