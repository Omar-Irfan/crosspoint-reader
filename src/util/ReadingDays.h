#pragma once

#include <cstdint>

// Calendar arithmetic for the reading log, free of storage and clock access.
// A "day" is the number of days since 1970-01-01.
namespace readinglog {

// A day counts as read from this many seconds; it only filters accidental opens.
constexpr uint16_t MIN_READ_DAY_SECONDS = 120;
// Shade thresholds in minutes: light below MEDIUM, dark below LONG, black from LONG.
constexpr uint16_t MEDIUM_DAY_MINUTES = 15;
constexpr uint16_t LONG_DAY_MINUTES = 45;

// Day number of a civil date (Howard Hinnant's days_from_civil).
inline uint32_t dayFromCivil(int year, const int month, const int dayOfMonth) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy =
      (153u * static_cast<unsigned>(month + (month > 2 ? -3 : 9)) + 2u) / 5u + static_cast<unsigned>(dayOfMonth) - 1u;
  const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return static_cast<uint32_t>(static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468);
}

// Civil date of a day number (civil_from_days).
inline void civilFromDay(const uint32_t day, int& year, int& month, int& dayOfMonth) {
  const int64_t z = static_cast<int64_t>(day) + 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  dayOfMonth = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  year = static_cast<int>(static_cast<int64_t>(yoe) + era * 400 + (month <= 2));
}

// Monday = 0 .. Sunday = 6. 1970-01-01 was a Thursday.
inline uint8_t weekdayOfDay(const uint32_t day) { return static_cast<uint8_t>((day + 3) % 7); }

// 0 = not read, 1 = light, 2 = medium, 3 = long.
inline uint8_t level(const uint16_t seconds) {
  if (seconds < MIN_READ_DAY_SECONDS) return 0;
  if (seconds < MEDIUM_DAY_MINUTES * 60) return 1;
  if (seconds < LONG_DAY_MINUTES * 60) return 2;
  return 3;
}

}  // namespace readinglog
