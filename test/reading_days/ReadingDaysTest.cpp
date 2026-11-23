#include <gtest/gtest.h>

#include "ReadingDays.h"

namespace {
using namespace readinglog;

TEST(ReadingDays, EpochIsDayZeroOnAThursday) {
  EXPECT_EQ(dayFromCivil(1970, 1, 1), 0u);
  EXPECT_EQ(weekdayOfDay(0), 3);  // Monday = 0
}

TEST(ReadingDays, KnownDates) {
  EXPECT_EQ(dayFromCivil(2026, 11, 23), 20780u);
  EXPECT_EQ(weekdayOfDay(20780), 0);  // a Monday
  EXPECT_EQ(dayFromCivil(2000, 3, 1) - dayFromCivil(2000, 2, 28), 2u);  // 2000 is a leap year
  EXPECT_EQ(dayFromCivil(2100, 3, 1) - dayFromCivil(2100, 2, 28), 1u);  // 2100 is not
}

TEST(ReadingDays, CivilRoundTripsAcrossManyYears) {
  for (uint32_t day = dayFromCivil(2020, 1, 1); day < dayFromCivil(2060, 1, 1); ++day) {
    int y, m, d;
    civilFromDay(day, y, m, d);
    ASSERT_EQ(dayFromCivil(y, m, d), day) << y << "-" << m << "-" << d;
  }
}

TEST(ReadingDays, ShadeLevelsFollowTheFixedThresholds) {
  EXPECT_EQ(level(0), 0);
  EXPECT_EQ(level(119), 0);        // under two minutes is not a read day
  EXPECT_EQ(level(120), 1);
  EXPECT_EQ(level(15 * 60 - 1), 1);
  EXPECT_EQ(level(15 * 60), 2);
  EXPECT_EQ(level(45 * 60 - 1), 2);
  EXPECT_EQ(level(45 * 60), 3);
  EXPECT_EQ(level(65535), 3);
}

}  // namespace
