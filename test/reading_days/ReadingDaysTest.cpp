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
  EXPECT_EQ(weekdayOfDay(20780), 0);                                    // a Monday
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
  EXPECT_EQ(level(119), 0);  // under two minutes is not a read day
  EXPECT_EQ(level(120), 1);
  EXPECT_EQ(level(15 * 60 - 1), 1);
  EXPECT_EQ(level(15 * 60), 2);
  EXPECT_EQ(level(45 * 60 - 1), 2);
  EXPECT_EQ(level(45 * 60), 3);
  EXPECT_EQ(level(65535), 3);
}

TEST(ReadingDays, TypicalDayNeedsAWeekOfReading) {
  const uint16_t six[] = {600, 0, 600, 600, 100, 600, 600, 600};  // 100 s is not a read day
  EXPECT_EQ(typicalDaySeconds(six, 8), 0);
  EXPECT_EQ(typicalDaySeconds(six, 0), 0);
}

TEST(ReadingDays, TypicalDayIsTheMedianReadDay) {
  const uint16_t days[] = {0, 3000, 600, 0, 1200, 2400, 1800, 3600, 900, 50};
  EXPECT_EQ(typicalDaySeconds(days, 10), 1800);  // 600 900 1200 [1800] 2400 3000 3600
  const uint16_t same[] = {2700, 2700, 2700, 2700, 2700, 2700, 2700};
  EXPECT_EQ(typicalDaySeconds(same, 7), 2700);
}

TEST(ReadingDays, ShadesAreRelativeToTheTypicalDay) {
  // A steady 45-minute reader: ordinary days are medium, only big days are black.
  EXPECT_EQ(level(45 * 60, 45 * 60), 2);
  EXPECT_EQ(level(22 * 60, 45 * 60), 1);
  EXPECT_EQ(level(67 * 60, 45 * 60), 2);
  EXPECT_EQ(level(68 * 60, 45 * 60), 3);
  EXPECT_EQ(level(100, 45 * 60), 0);
}

}  // namespace
