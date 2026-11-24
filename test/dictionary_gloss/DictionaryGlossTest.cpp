#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "DictionaryGloss.h"
#include "HtmlToPlainText.h"

namespace {
constexpr char MARKER = '\x01';
}

TEST(DictionaryGloss, SplitsOnWhitespace) {
  std::vector<std::string> w;
  DictionaryGloss::splitWords("  فقرأوه   وكتبوا\tعليه\n", w);
  ASSERT_EQ(w.size(), 3u);
  EXPECT_EQ(w[0], "فقرأوه");
  EXPECT_EQ(w[2], "عليه");
}

TEST(DictionaryGloss, SplitIsBounded) {
  std::string many;
  for (int i = 0; i < 40; ++i) many += "كلمة ";
  std::vector<std::string> w;
  DictionaryGloss::splitWords(many, w);
  EXPECT_EQ(w.size(), DictionaryGloss::MAX_WORDS);
}

TEST(DictionaryGloss, SkipsLabelsHeadingsAndPronunciation) {
  // Shape of a merged Wiktionary entry after htmlToPlainText.
  const std::string html =
      "<p><b>[Wiktionary]</b> كِتَاب</p><h4>Noun</h4><p>IPA: /ki.taːb/</p>"
      "<p>Dialectal variation from the aforementioned:</p>"
      "<ol><li>book</li><li>letter, note</li><li>record</li></ol>";
  const std::string plain = htmlToPlainText(html, MARKER);
  EXPECT_EQ(DictionaryGloss::snippet(plain, MARKER), "book; letter, note");
}

TEST(DictionaryGloss, SkipsInflectionLine) {
  const std::string plain = "[Wiktionary] فضل\nفَضَلَ • (faḍala) I, non-past يَفْضُلُ\n(intransitive) to remain\n";
  EXPECT_EQ(DictionaryGloss::snippet(plain, MARKER), "(intransitive) to remain");
}

TEST(DictionaryGloss, ReportsSourceOfSnippet) {
  const std::string plain = "[Wiktionary] فضل\n(intransitive) to remain\n[المعجم (Arramooz)] فضل\nزيادة\n";
  std::string source;
  // The snippet stops at the next source, so its lines all match the label.
  EXPECT_EQ(DictionaryGloss::snippet(plain, MARKER, &source), "(intransitive) to remain");
  EXPECT_EQ(source, "Wiktionary");
  DictionaryGloss::snippet("plain text only\n", MARKER, &source);
  EXPECT_EQ(source, "");
}

TEST(DictionaryGloss, ArabicDefinition) {
  const std::string plain = "[المعجم] مدرسة\nمَدْرَسَةٌ — جامد، جذر: درس\nمُؤَسَّسَةٌ تَعْلِيمِيَّةٌ\n";
  EXPECT_EQ(DictionaryGloss::snippet(plain, MARKER), "مَدْرَسَةٌ — جامد، جذر: درس; مُؤَسَّسَةٌ تَعْلِيمِيَّةٌ");
}

TEST(DictionaryGloss, CutsOnUtf8Boundary) {
  std::string plain;
  for (int i = 0; i < 200; ++i) plain += "ب";
  const std::string s = DictionaryGloss::snippet(plain, MARKER);
  EXPECT_LE(s.size(), DictionaryGloss::MAX_SNIPPET_BYTES + 3);  // + "…"
  EXPECT_EQ(s.substr(s.size() - 3), "…");
  EXPECT_EQ(s.size() % 2, 1u);  // 2-byte letters + 3-byte ellipsis: no split letter
}

TEST(DictionaryGloss, EmptyWhenOnlyNoise) { EXPECT_EQ(DictionaryGloss::snippet("[X] y\nIPA: /a/\n", MARKER), ""); }

// A merged entry for أهل: a jussive form of another verb first, the noun later.
const std::string AHL =
    "[Wiktionary] أهل\nأَهُلْ (ʔahul): first-person singular non-past active jussive of هَالَ (hāla)\n"
    "[Wiktionary] أَهَل\nverbal noun of أَهِلَ (ʔahila) (form I)\nfamiliarization (with)\n"
    "[Wiktionary] أَهْل\nfamily, household; people, kinfolk, relations\ninhabitants\n";

TEST(DictionaryGloss, SkipsGrammaticalFormLines) {
  EXPECT_EQ(DictionaryGloss::snippet(AHL, MARKER), "verbal noun of أَهِلَ (ʔahila) (form I); familiarization (with)");
  // Used when nothing else is there.
  EXPECT_EQ(DictionaryGloss::snippet("plural of كتاب\n", MARKER), "plural of كتاب");
}

TEST(DictionaryGloss, PrefersSectionMatchingStem) {
  std::string source;
  EXPECT_EQ(DictionaryGloss::snippet(AHL, MARKER, &source, "أَهل"),
            "family, household; people, kinfolk, relations; inhabitants");
  EXPECT_EQ(source, "Wiktionary");
  // No section matches: the whole entry is used.
  EXPECT_EQ(DictionaryGloss::snippet(AHL, MARKER, nullptr, "كِتاب"),
            "verbal noun of أَهِلَ (ʔahila) (form I); familiarization (with)");
}

TEST(DictionaryGloss, MatchesStemsWrittenWithoutFathaBeforeAlef) {
  const std::string plain = "[Wiktionary] كُتَّاب\nwriters\n[Wiktionary] كِتَاب\nbook\n[Wiktionary] اِسْتَقَالَ\nto resign\n";
  EXPECT_EQ(DictionaryGloss::snippet(plain, MARKER, nullptr, "كِتاب"), "book");
  EXPECT_EQ(DictionaryGloss::snippet(plain, MARKER, nullptr, "ٱِستَقال"), "to resign");
}

TEST(DictionaryGloss, MatchedSectionDoesNotBorrowTheNextOne) {
  // The matched section only says "plural of": that line is used, not the next word's sense.
  const std::string plain = "[Wiktionary] كُتُب\nplural of كِتَاب\n[Wiktionary] كَتَبَ\nto write\n";
  EXPECT_EQ(DictionaryGloss::snippet(plain, MARKER, nullptr, "كُتُب"), "plural of كِتَاب");
}
