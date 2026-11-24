#include <ArabicMorph.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>

namespace {

// A tiny data file in the build script's format.
const std::string DATA =
    "#ARABIC_MORPH 2\n"
    "P\t\t\tPref-0\n"
    "P\tو\tوَ\tPref-Wa\n"
    "P\tوب\tوَ+بِ\tNPref-Bi\n"
    "S\t\t\tSuff-0\n"
    "S\tهم\tهِم\tNSuff-h\n"
    "S\tوا\tُوا\tPVSuff-uwA\n"
    "AB\tPref-0\tN\n"
    "AB\tPref-0\tPV\n"
    "AB\tPref-Wa\tPV\n"
    "AB\tNPref-Bi\tN\n"
    "AC\tPref-0\tSuff-0\n"
    "AC\tPref-0\tNSuff-h\n"
    "AC\tPref-0\tPVSuff-uwA\n"
    "AC\tPref-Wa\tPVSuff-uwA\n"
    "AC\tNPref-Bi\tNSuff-h\n"
    "BC\tN\tSuff-0\n"
    "BC\tN\tNSuff-h\n"
    "BC\tPV\tPVSuff-uwA\n"
    "I\tباب\t0\n"
    "I\tكتاب\t23\n"  // second block: كتاب and كتب
    "#STEMS\n"
    "باب\tباب N بوب\n"
    "كتاب\tكِتاب N كتب\n"
    "كتب\tكَتَب PV كتب\n";

ArabicMorph::ReadAt reader(std::shared_ptr<const std::string> data) {
  return [data](uint32_t offset, char* buf, int len) {
    if (offset >= data->size()) return 0;
    const size_t n = std::min<size_t>(len, data->size() - offset);
    memcpy(buf, data->data() + offset, n);
    return static_cast<int>(n);
  };
}

struct Table {
  std::shared_ptr<const std::string> data;
  ArabicMorph::ReadAt read;
  ArabicMorph::Analyzer analyzer;
  explicit Table(const std::string& text) : data(std::make_shared<std::string>(text)), read(reader(data)) {
    EXPECT_TRUE(analyzer.open(static_cast<uint32_t>(data->size()), read));
  }
  std::vector<ArabicMorph::Analysis> analyze(std::string_view word, size_t max = 2) const {
    std::vector<ArabicMorph::Analysis> out;
    analyzer.analyze(word, read, out, max);
    return out;
  }
};

TEST(ArabicMorph, NormalizeDropsMarksAndTatweel) {
  EXPECT_EQ(ArabicMorph::normalize("وَبِكِتَـابِهِمْ"), "وبكتابهم");
  EXPECT_EQ(ArabicMorph::normalize("ٱلْكِتَاب"), "الكتاب");
}

TEST(ArabicMorph, SplitsPrefixesStemAndSuffix) {
  const Table t(DATA);
  const auto a = t.analyze("وَبِكِتَابِهِمْ");
  ASSERT_EQ(a.size(), 1u);
  EXPECT_EQ(a[0].breakdown, "وَ + بِ + كِتاب + هِم");
  EXPECT_EQ(a[0].root, "ك ت ب");
}

TEST(ArabicMorph, BareStem) {
  const Table t(DATA);
  const auto a = t.analyze("باب");
  ASSERT_EQ(a.size(), 1u);
  EXPECT_EQ(a[0].breakdown, "باب");
  EXPECT_EQ(a[0].root, "ب و ب");
}

TEST(ArabicMorph, SuffixMarkMovesOntoStem) {
  const Table t(DATA);
  const auto a = t.analyze("وكتبوا");
  ASSERT_EQ(a.size(), 1u);
  EXPECT_EQ(a[0].breakdown, "وَ + كَتَبُ + وا");
}

TEST(ArabicMorph, RejectsDisallowedCombination) {
  const Table t(DATA);
  EXPECT_TRUE(t.analyze("وبكتب").empty());  // بِ is not allowed before a verb
  EXPECT_TRUE(t.analyze("قلم").empty());    // no such stem
  EXPECT_TRUE(t.analyze("book").empty());
}

TEST(ArabicMorph, RejectsOtherFiles) {
  ArabicMorph::Analyzer analyzer;
  for (const std::string bad : {std::string("hello\n#STEMS\n"), std::string("#ARABIC_MORPH 1\n#STEMS\n"),
                                std::string("#ARABIC_MORPH 2\nP\t\t\tPref-0\n#STEMS\n")}) {  // no blocks
    const auto data = std::make_shared<const std::string>(bad);
    EXPECT_FALSE(analyzer.open(static_cast<uint32_t>(bad.size()), reader(data)));
    EXPECT_FALSE(analyzer.isOpen());
  }
}

TEST(ArabicMorph, StopsReadingAWrongFileEarly) {
  // A large file that is not a table is rejected on its first chunk.
  std::string big(1024 * 1024, 'x');
  const auto data = std::make_shared<const std::string>(big);
  int reads = 0;
  ArabicMorph::Analyzer analyzer;
  EXPECT_FALSE(analyzer.open(static_cast<uint32_t>(big.size()), [&](uint32_t o, char* b, int l) {
    ++reads;
    return reader(data)(o, b, l);
  }));
  EXPECT_EQ(reads, 1);
  // A table header that never ends stops at the cap.
  std::string endless = "#ARABIC_MORPH 2\n" + std::string(ArabicMorph::MAX_HEADER_BYTES + 8192, 'x');
  const auto endlessData = std::make_shared<const std::string>(endless);
  EXPECT_FALSE(analyzer.open(static_cast<uint32_t>(endless.size()), reader(endlessData)));
}

TEST(ArabicMorph, OneReadPerBlock) {
  const auto data = std::make_shared<const std::string>(DATA);
  ArabicMorph::Analyzer analyzer;
  ASSERT_TRUE(analyzer.open(static_cast<uint32_t>(data->size()), reader(data)));
  int reads = 0;
  const ArabicMorph::ReadAt counting = [&](uint32_t o, char* b, int l) {
    ++reads;
    return reader(data)(o, b, l);
  };
  std::vector<ArabicMorph::Analysis> out;
  analyzer.analyze("كتاب", counting, out, 2);  // كتاب and كتب share the second block
  EXPECT_EQ(reads, 1);
}

// Runs against the full generated table when ARABIC_MORPH_DATA points at it.
TEST(ArabicMorph, FullTable) {
  const char* path = std::getenv("ARABIC_MORPH_DATA");
  if (!path) GTEST_SKIP() << "set ARABIC_MORPH_DATA to the generated arabic_morph.txt";
  std::ifstream in(path, std::ios::binary);
  std::stringstream ss;
  ss << in.rdbuf();
  const Table t(ss.str());
  auto a = t.analyze("وَبِكِتَابِهِمْ");
  ASSERT_FALSE(a.empty());
  EXPECT_EQ(a[0].root, "ك ت ب");
  for (const char* w : {"وبكتابهم", "فسيكفيكهم", "والمؤمنون", "يعلمون", "استغفروا", "بسم", "الرحمن", "رسول", "قال"}) {
    a = t.analyze(w);
    printf("%s ->", w);
    for (const auto& x : a) printf("  [%s | %s]", x.breakdown.c_str(), x.root.c_str());
    printf("\n");
  }
}

}  // namespace
