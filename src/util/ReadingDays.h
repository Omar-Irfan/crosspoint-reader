#pragma once

#include <cstddef>
#include <cstdint>

// Calendar arithmetic for the reading log, free of storage and clock access.
// A "day" is the number of days since 1970-01-01.
namespace readinglog {

// A day counts as read from this many seconds; it only filters accidental opens.
constexpr uint16_t MIN_READ_DAY_SECONDS = 120;
// Shades are relative to the reader's typical day (the median of read days): light
// below half of it, black from one and a half times it. Until enough days are read
// to know a typical day, these fixed minutes are used instead.
constexpr uint16_t MEDIUM_DAY_MINUTES = 15;
constexpr uint16_t LONG_DAY_MINUTES = 45;
constexpr int MIN_DAYS_FOR_TYPICAL = 7;

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

// Median seconds of the read days in seconds[0..count), or 0 when fewer than
// MIN_DAYS_FOR_TYPICAL days were read. Quadratic, no copy: count is at most 366.
inline uint16_t typicalDaySeconds(const uint16_t* seconds, const size_t count) {
  int readDays = 0;
  for (size_t i = 0; i < count; ++i) readDays += seconds[i] >= MIN_READ_DAY_SECONDS;
  if (readDays < MIN_DAYS_FOR_TYPICAL) return 0;
  const int middle = (readDays + 1) / 2;  // the lower median
  uint16_t best = 0xFFFF;
  for (size_t i = 0; i < count; ++i) {
    const uint16_t v = seconds[i];
    if (v < MIN_READ_DAY_SECONDS || v >= best) continue;
    int atMost = 0;
    for (size_t k = 0; k < count; ++k) atMost += seconds[k] >= MIN_READ_DAY_SECONDS && seconds[k] <= v;
    if (atMost >= middle) best = v;
  }
  return best;
}

// 0 = not read, 1 = light, 2 = medium, 3 = a big day. typical 0 uses the fixed minutes.
inline uint8_t level(const uint16_t seconds, const uint16_t typical = 0) {
  if (seconds < MIN_READ_DAY_SECONDS) return 0;
  const uint32_t lightBelow = typical > 0 ? typical / 2u : MEDIUM_DAY_MINUTES * 60u;
  const uint32_t blackFrom = typical > 0 ? typical * 3u / 2u : LONG_DAY_MINUTES * 60u;
  if (seconds < lightBelow) return 1;
  if (seconds < blackFrom) return 2;
  return 3;
}

}  // namespace readinglog
