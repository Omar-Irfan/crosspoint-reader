#include "ArabicStem.h"

#include <algorithm>
#include <cstring>

namespace ArabicStem {
namespace {

// Every Arabic letter and mark used here is a 2-byte UTF-8 sequence, so byte
// prefix/suffix comparisons are safe and stems are measured in bytes.
constexpr size_t MIN_STEM_BYTES = 4;  // two letters

constexpr const char* SHADDA = "\xD9\x91";   // U+0651
constexpr const char* TATWEEL = "\xD9\x80";  // U+0640

struct Proclitic {
  const char* text;
  uint8_t particles;  // attached words removed: و، ب، ال each count one
  bool futureOnly;    // س: only before an imperfect-verb prefix (ي ت ن أ)
};

constexpr Proclitic PROCLITICS[] = {
    {"ال", 1, false},   {"و", 1, false},   {"ف", 1, false},    {"ب", 1, false},    {"ك", 1, false},
    {"ل", 1, false},    {"س", 1, true},    {"لل", 2, false},   {"وال", 2, false},  {"فال", 2, false},
    {"بال", 2, false},  {"كال", 2, false}, {"وب", 2, false},   {"فب", 2, false},   {"وك", 2, false},
    {"فك", 2, false},   {"ول", 2, false},  {"فل", 2, false},   {"وس", 2, true},    {"فس", 2, true},
    {"ولل", 3, false},  {"فلل", 3, false}, {"وبال", 3, false}, {"فبال", 3, false}, {"وكال", 3, false},
    {"فكال", 3, false},
};

constexpr const char* ENCLITICS[] = {"كما", "هما", "كم", "كن", "هم", "هن", "ها", "نا", "ني", "ه", "ك", "ي"};

bool startsWith(const std::string& s, const char* p) {
  const size_t n = strlen(p);
  return s.size() >= n && s.compare(0, n, p) == 0;
}

bool endsWith(const std::string& s, const char* p) {
  const size_t n = strlen(p);
  return s.size() >= n && s.compare(s.size() - n, n, p) == 0;
}

void eraseAll(std::string& s, const char* token) {
  const size_t n = strlen(token);
  for (size_t pos = s.find(token); pos != std::string::npos; pos = s.find(token, pos)) s.erase(pos, n);
}

// أ إ آ ٱ -> ا
std::string foldHamza(const std::string& s) {
  std::string out = s;
  for (size_t i = 0; i + 1 < out.size(); ++i) {
    const auto c = static_cast<unsigned char>(out[i]);
    const auto n = static_cast<unsigned char>(out[i + 1]);
    if ((c == 0xD8 && (n == 0xA2 || n == 0xA3 || n == 0xA5)) || (c == 0xD9 && n == 0xB1)) {
      out[i] = static_cast<char>(0xD8);
      out[i + 1] = static_cast<char>(0xA7);
    }
  }
  return out;
}

bool isImperfectStart(const std::string& s) {
  return startsWith(s, "ي") || startsWith(s, "ت") || startsWith(s, "ن") || startsWith(s, "أ");
}

struct Candidate {
  uint8_t cost;  // spelling changes from the looked-up word
  std::string text;
};

void addCandidate(std::vector<Candidate>& out, uint8_t cost, std::string text) {
  if (text.size() >= MIN_STEM_BYTES && out.size() < MAX_CANDIDATES) out.push_back({cost, std::move(text)});
}

// The stem left by removing a pronoun suffix, plus the spelling the suffix changed.
void addSuffixStems(std::vector<Candidate>& out, uint8_t cost, const std::string& r) {
  addCandidate(out, cost, r);
  if (endsWith(r, "ت")) addCandidate(out, cost, r.substr(0, r.size() - 2) + "ة");  // مدرستهم -> مدرسة
  if (endsWith(r, "و")) addCandidate(out, cost, r + "ا");                          // كتبوه -> كتبوا
  if (endsWith(r, "ا")) addCandidate(out, cost, r.substr(0, r.size() - 2) + "ى");  // هداه -> هدى
  if (endsWith(r, "ؤ") || endsWith(r, "ئ")) addCandidate(out, cost, r.substr(0, r.size() - 2) + "ء");
}

void addEncliticStems(std::vector<Candidate>& out, uint8_t cost, const std::string& w) {
  for (const char* e : ENCLITICS) {
    if (endsWith(w, e) && w.size() >= strlen(e) + MIN_STEM_BYTES) {
      addSuffixStems(out, cost + 1, w.substr(0, w.size() - strlen(e)));
    }
  }
}

// Every proclitic split of w, each also stripped of an enclitic.
void addCliticStems(std::vector<Candidate>& out, uint8_t cost, const std::string& w) {
  addEncliticStems(out, cost, w);
  for (const auto& p : PROCLITICS) {
    if (!startsWith(w, p.text) || w.size() < strlen(p.text) + MIN_STEM_BYTES) continue;
    std::string rest = w.substr(strlen(p.text));
    if (p.futureOnly && !isImperfectStart(rest)) continue;
    const auto c = static_cast<uint8_t>(cost + p.particles);
    addEncliticStems(out, c, rest);
    addCandidate(out, c, std::move(rest));
  }
}

}  // namespace

bool containsArabic(const std::string_view word) {
  for (size_t i = 0; i + 1 < word.size(); ++i) {
    const auto c = static_cast<unsigned char>(word[i]);
    const auto n = static_cast<unsigned char>(word[i + 1]);
    if ((c == 0xD8 && n >= 0xA1) || (c == 0xD9 && n <= 0x8A) || (c == 0xD9 && n >= 0xB1) || c == 0xDA ||
        (c == 0xDB && n <= 0x93)) {
      return true;
    }
  }
  return false;
}

void variants(const std::string& word, std::vector<std::string>& out) {
  out.clear();
  std::string base = word;
  eraseAll(base, TATWEEL);
  std::string bare = base;
  eraseAll(bare, SHADDA);
  const std::string folded = foldHamza(bare);

  std::vector<Candidate> candidates;
  candidates.reserve(MAX_CANDIDATES);
  addCandidate(candidates, 0, base);
  addCandidate(candidates, 0, bare);
  addCliticStems(candidates, 0, bare);
  if (folded != bare) {
    addCandidate(candidates, 1, folded);
    addCliticStems(candidates, 1, folded);
  }
  // Fewest changes first; generation order breaks ties.
  std::stable_sort(candidates.begin(), candidates.end(),
                   [](const Candidate& a, const Candidate& b) { return a.cost < b.cost; });

  out.reserve(MAX_VARIANTS);
  for (auto& c : candidates) {
    if (out.size() >= MAX_VARIANTS) break;
    if (c.text == word || std::find(out.begin(), out.end(), c.text) != out.end()) continue;
    out.push_back(std::move(c.text));
  }
}

}  // namespace ArabicStem
