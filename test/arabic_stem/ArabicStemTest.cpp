#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "ArabicStem.h"

namespace {

std::vector<std::string> variantsOf(const std::string& w) {
  std::vector<std::string> out;
  ArabicStem::variants(w, out);
  return out;
}

bool has(const std::vector<std::string>& v, const std::string& s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

size_t indexOf(const std::vector<std::string>& v, const std::string& s) {
  return static_cast<size_t>(std::find(v.begin(), v.end(), s) - v.begin());
}

}  // namespace

TEST(ArabicStem, DetectsArabic) {
  EXPECT_TRUE(ArabicStem::containsArabic("كتاب"));
  EXPECT_TRUE(ArabicStem::containsArabic("word كتاب"));
  EXPECT_FALSE(ArabicStem::containsArabic("book"));
  EXPECT_FALSE(ArabicStem::containsArabic("\xD8\x8C"));  // Arabic comma alone is not a letter
}

TEST(ArabicStem, Punctuation) {
  EXPECT_TRUE(ArabicStem::isArabicPunctuation(0x060C));   // ،
  EXPECT_TRUE(ArabicStem::isArabicPunctuation(0x061F));   // ؟
  EXPECT_TRUE(ArabicStem::isArabicPunctuation(0x061B));   // ؛
  EXPECT_FALSE(ArabicStem::isArabicPunctuation(0x0643));  // ك
}

TEST(ArabicStem, PronounSuffixes) {
  EXPECT_TRUE(has(variantsOf("كتابهم"), "كتاب"));
  EXPECT_TRUE(has(variantsOf("كتابها"), "كتاب"));
  EXPECT_TRUE(has(variantsOf("شروحهم"), "شروح"));
}

TEST(ArabicStem, SuffixRestoresTaMarbuta) { EXPECT_TRUE(has(variantsOf("مدرستهم"), "مدرسة")); }

TEST(ArabicStem, SuffixRestoresVerbAlif) {
  EXPECT_TRUE(has(variantsOf("كتبوه"), "كتبوا"));
  EXPECT_TRUE(has(variantsOf("فقرأوه"), "قرأوا"));
}

TEST(ArabicStem, Prefixes) {
  EXPECT_TRUE(has(variantsOf("وقال"), "قال"));
  EXPECT_TRUE(has(variantsOf("والمدرسة"), "مدرسة"));
  EXPECT_TRUE(has(variantsOf("والمدرسة"), "المدرسة"));
  EXPECT_TRUE(has(variantsOf("وبالكتاب"), "كتاب"));
  EXPECT_TRUE(has(variantsOf("للمسلمين"), "مسلمين"));
  EXPECT_TRUE(has(variantsOf("لطلاب"), "طلاب"));
}

TEST(ArabicStem, PrefixAndSuffix) {
  EXPECT_TRUE(has(variantsOf("بكتابه"), "كتاب"));
  EXPECT_TRUE(has(variantsOf("بعلمه"), "علم"));
}

TEST(ArabicStem, FutureOnlyBeforeImperfect) {
  EXPECT_TRUE(has(variantsOf("سيكتبون"), "يكتبون"));
  EXPECT_FALSE(has(variantsOf("سلام"), "لام"));
}

TEST(ArabicStem, ShaddaAndHamza) {
  EXPECT_EQ(indexOf(variantsOf("كلّ"), "كل"), 0u);  // fewest changes first
  EXPECT_TRUE(has(variantsOf("أكل"), "اكل"));
  EXPECT_TRUE(has(variantsOf("ـكتـاب"), "كتاب"));  // tatweel
}

TEST(ArabicStem, FewestChangesFirst) {
  const auto v = variantsOf("وكتابهم");
  EXPECT_LT(indexOf(v, "كتابهم"), indexOf(v, "كتاب"));
}

TEST(ArabicStem, SingleParticleBeforeDouble) {
  const auto a = variantsOf("وكتبوا");
  EXPECT_LT(indexOf(a, "كتبوا"), indexOf(a, "تبوا"));
  const auto b = variantsOf("فكتبوه");
  EXPECT_LT(indexOf(b, "كتبوا"), indexOf(b, "تبوه"));
}

TEST(ArabicStem, Bounds) {
  const auto v = variantsOf("وبالكتابهما");
  EXPECT_LE(v.size(), ArabicStem::MAX_VARIANTS);
  EXPECT_FALSE(has(v, "وبالكتابهما"));
  for (const auto& s : v) EXPECT_GE(s.size(), 4u);
  EXPECT_TRUE(variantsOf("و").empty());
}
