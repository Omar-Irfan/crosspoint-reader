#include "DictionaryGloss.h"

#include <cstring>

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

std::string snippet(const std::string& plain, const char headingMarker, std::string* source) {
  std::string out;
  if (source) source->clear();
  int kept = 0;
  size_t pos = 0;
  while (pos < plain.size() && kept < 2) {
    size_t end = plain.find('\n', pos);
    if (end == std::string::npos) end = plain.size();
    const std::string line = trim(plain.substr(pos, end - pos));
    pos = end + 1;
    if (kept > 0 && !line.empty() && line[0] == '[') break;  // the next source's lines: keep one label
    if (kept == 0 && source && !line.empty() && line[0] == '[') {
      const size_t close = line.find(']');
      if (close != std::string::npos) *source = line.substr(1, close - 1);
    }
    if (isNoiseLine(line, headingMarker)) continue;
    if (kept > 0) out += "; ";
    out += line;
    ++kept;
  }
  cutUtf8(out, MAX_SNIPPET_BYTES);
  return out;
}

}  // namespace DictionaryGloss
