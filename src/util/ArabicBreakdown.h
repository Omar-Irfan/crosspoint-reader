#pragma once

#include <ArabicMorph.h>

#include <string>
#include <vector>

// Word breakdowns for the dictionary page, from /.crosspoint/arabic_morph.txt
// (built by scripts/build_arabic_morph.py; see lib/ArabicMorph). Without the
// file, or on boards without PSRAM, nothing is shown.
namespace ArabicBreakdown {

// Up to maxReadings readings of one Arabic word; empty when the word is not
// one Arabic word or is not recognized.
std::vector<ArabicMorph::Analysis> analyze(const std::string& word, size_t maxReadings);

// "وَ + بِ + كِتاب + هم  (ك ت ب)": the breakdown with its root, when known.
std::string line(const ArabicMorph::Analysis& reading);

// "  (ك ت ب)", or "" when the root is unknown.
std::string rootSuffix(const ArabicMorph::Analysis& reading);

}  // namespace ArabicBreakdown
