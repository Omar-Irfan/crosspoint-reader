#pragma once

#include <cstddef>
#include <cstdint>

#include "ReadingDays.h"

// Seconds read per local calendar day, plus an all-time summary, kept in one
// small file on the SD card. Written only when a reading session ends (exit or
// sleep), never per page.
namespace readinglog {

// ponytail: the sleep screen shows 18 weeks (a 450 px grid on a 480 px panel); the Year page shows older days.
constexpr size_t HISTORY_WEEKS = 18;
constexpr size_t HISTORY_DAYS = HISTORY_WEEKS * 7;

struct Summary {
  uint32_t today = 0;     // local day number (days since 1970-01-01)
  uint32_t startDay = 0;  // Monday HISTORY_WEEKS - 1 weeks before this week's Monday
  uint16_t seconds[HISTORY_DAYS] = {};  // seconds read, index = day - startDay
  uint32_t hourSeconds[24] = {};        // all-time, by local hour of the session's end
  uint32_t sessions = 0;
  uint32_t totalSeconds = 0;
  bool valid = false;  // false when the clock is untrusted or nothing is stored yet
};

// Local calendar day number for a UTC epoch; 0 when the epoch is 0 or implausible.
uint32_t localDay(int64_t utcEpoch);
// Credits seconds read, ending at utcEpoch, to that local day and hour.
bool add(int64_t utcEpoch, uint32_t seconds);
// Fills out from the file; today comes from nowEpoch. Returns out.valid.
bool load(int64_t nowEpoch, Summary& out);

// One calendar year of the log, for the Year page.
struct YearSummary {
  int year = 0;
  int firstYear = 0;  // earliest year with a record
  int lastYear = 0;   // the current year
  uint32_t jan1 = 0;  // day number of January 1st
  uint16_t days = 365;
  uint16_t seconds[366] = {};  // index = day - jan1
  bool valid = false;
};
// Reads the whole log once; year 0 means the current year.
bool loadYear(int64_t nowEpoch, int year, YearSummary& out);

}  // namespace readinglog
