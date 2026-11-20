#include "ArabicBreakdown.h"

#include <ArabicStem.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdint>

namespace ArabicBreakdown {
namespace {

#ifdef BOARD_HAS_PSRAM
constexpr const char* MORPH_FILE = "/.crosspoint/arabic_morph.txt";

// The header tables (about 210 KB with their indexes, in PSRAM) load on the first
// Arabic lookup and stay for the session. The file is opened for each call
// only; a changed file (other size) is loaded again.
ArabicMorph::Analyzer& analyzer() {
  static ArabicMorph::Analyzer instance;
  return instance;
}

void analyzeFile(const std::string& word, const size_t maxReadings, std::vector<ArabicMorph::Analysis>& out) {
  static uint32_t rejectedSize = UINT32_MAX;  // a file that failed to load is not retried until it changes
  HalFile file;
  if (!Storage.openFileForRead("MORPH", MORPH_FILE, file)) return;
  const auto size = static_cast<uint32_t>(file.fileSize());
  const ArabicMorph::ReadAt read = [&file](const uint32_t offset, char* buf, const int len) {
    return file.seekSet(offset) ? file.read(buf, len) : -1;
  };
  auto& a = analyzer();
  if ((!a.isOpen() || a.loadedFileSize() != size) && size != rejectedSize) {
    if (!a.open(size, read)) {
      LOG_ERR("MORPH", "%s is not a word-breakdown table", MORPH_FILE);
      rejectedSize = size;
    }
  }
  if (a.isOpen() && a.loadedFileSize() == size) a.analyze(word, read, out, maxReadings);
  file.close();
}
#else
// The tables need PSRAM; boards without it show no breakdown.
void analyzeFile(const std::string&, size_t, std::vector<ArabicMorph::Analysis>&) {}
#endif

}  // namespace

std::vector<ArabicMorph::Analysis> analyze(const std::string& word, const size_t maxReadings) {
  std::vector<ArabicMorph::Analysis> out;
  if (ArabicStem::containsArabic(word) && word.find_first_of(" \t\r\n") == std::string::npos) {
    analyzeFile(word, maxReadings, out);
  }
  return out;
}

std::string rootSuffix(const ArabicMorph::Analysis& reading) {
  return reading.root.empty() ? std::string{} : "  (" + reading.root + ")";
}

std::string line(const ArabicMorph::Analysis& reading) { return reading.breakdown + rootSuffix(reading); }

}  // namespace ArabicBreakdown
