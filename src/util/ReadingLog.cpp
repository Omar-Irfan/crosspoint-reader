#include "ReadingLog.h"

#include <HalStorage.h>
#include <Logging.h>
#include <common/FsApiConstants.h>

#include <algorithm>
#include <ctime>

namespace readinglog {
namespace {
constexpr const char* PATH = "/.crosspoint/reading_log.bin";
constexpr uint8_t MAGIC[4] = {'R', 'L', 'G', '1'};
constexpr size_t HEADER_BYTES = 4 + 4 + 4 + 24 * 4;
constexpr size_t RECORD_BYTES = 6;  // u32 day, u16 seconds, little endian

void put32(uint8_t* p, const uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint32_t get32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
         static_cast<uint32_t>(p[3]) << 24;
}

bool localTm(const int64_t utcEpoch, struct tm& out) {
  if (utcEpoch < 1577836800) return false;  // before 2020: the clock was not set
  const time_t t = static_cast<time_t>(utcEpoch);
  return localtime_r(&t, &out) != nullptr;
}

bool readHeader(HalFile& f, uint8_t* header) {
  return f.size() >= HEADER_BYTES && f.seek(0) && f.read(header, HEADER_BYTES) == static_cast<int>(HEADER_BYTES) &&
         std::equal(MAGIC, MAGIC + 4, header);
}

// A file that is too short to hold a header is simply new (or left empty by a failed write).
// A full-size file that fails to read is a transient error and must not wipe the totals, and one
// with a foreign header is removed so its bytes are never read as records.
enum class OpenState { Ready, Fresh, Unreadable };
OpenState prepare(HalFile& f, uint8_t* header) {
  if (f.size() < HEADER_BYTES) return OpenState::Fresh;
  if (readHeader(f, header)) return OpenState::Ready;
  uint8_t probe[4] = {};
  if (!f.seek(0) || f.read(probe, sizeof(probe)) != static_cast<int>(sizeof(probe))) return OpenState::Unreadable;
  return std::equal(MAGIC, MAGIC + 4, probe) ? OpenState::Unreadable : OpenState::Fresh;
}
}  // namespace

uint32_t localDay(const int64_t utcEpoch) {
  struct tm tm;
  if (!localTm(utcEpoch, tm)) return 0;
  return dayFromCivil(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

bool add(const int64_t utcEpoch, const uint32_t seconds) {
  struct tm tm;
  if (seconds == 0 || !localTm(utcEpoch, tm)) return false;
  const uint32_t day = dayFromCivil(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

  HalFile f = Storage.open(PATH, O_RDWR | O_CREAT);
  if (!f) {
    LOG_ERR("RLOG", "Cannot open %s", PATH);
    return false;
  }
  uint8_t header[HEADER_BYTES] = {};
  const OpenState state = prepare(f, header);
  if (state == OpenState::Unreadable) {
    LOG_ERR("RLOG", "Cannot read %s", PATH);
    return false;
  }
  if (state == OpenState::Fresh) {
    if (f.size() > 0) {  // foreign or truncated content: start the log over
      f.close();
      Storage.remove(PATH);
      f = Storage.open(PATH, O_RDWR | O_CREAT);
      if (!f) return false;
    }
    std::fill(header, header + HEADER_BYTES, 0);
    std::copy(MAGIC, MAGIC + 4, header);
  }
  put32(header + 4, get32(header + 4) + 1);
  put32(header + 8, get32(header + 8) + seconds);
  uint8_t* hourSlot = header + 12 + 4 * tm.tm_hour;
  put32(hourSlot, get32(hourSlot) + seconds);

  // The last record is today's when the day matches; otherwise append.
  uint8_t record[RECORD_BYTES];
  size_t recordPos = HEADER_BYTES;
  uint32_t lastDay = 0;
  uint32_t lastSeconds = 0;
  const size_t size = f.size();
  if (size >= HEADER_BYTES + RECORD_BYTES) {
    const size_t count = (size - HEADER_BYTES) / RECORD_BYTES;
    recordPos = HEADER_BYTES + (count - 1) * RECORD_BYTES;
    if (f.seek(recordPos) && f.read(record, RECORD_BYTES) == static_cast<int>(RECORD_BYTES)) {
      lastDay = get32(record);
      lastSeconds = static_cast<uint32_t>(record[4]) | static_cast<uint32_t>(record[5]) << 8;
    }
    if (lastDay == 0 || day > lastDay) recordPos += RECORD_BYTES;  // new day: append
  }
  // ponytail: one session is credited to a single day (the last render's), so one that crosses midnight
  // lands on the later day; split it per day if that ever matters.
  // A clock that stepped back keeps crediting the newest day, so the log stays sorted.
  const uint32_t writeDay = lastDay != 0 && day < lastDay ? lastDay : day;
  const uint32_t total = std::min<uint32_t>(65535, (writeDay == lastDay ? lastSeconds : 0) + seconds);
  put32(record, writeDay);
  record[4] = static_cast<uint8_t>(total & 0xFF);
  record[5] = static_cast<uint8_t>(total >> 8);

  // Header first: on the SD card a seek past the end of the file fails, so a new
  // file must be extended to the header before its first record is written.
  const bool ok = f.seek(0) && f.write(header, HEADER_BYTES) == HEADER_BYTES && f.seek(recordPos) &&
                  f.write(record, RECORD_BYTES) == RECORD_BYTES;
  f.close();
  if (!ok) LOG_ERR("RLOG", "Write failed");
  return ok;
}

bool load(const int64_t nowEpoch, Summary& out) {
  out = Summary();
  out.today = localDay(nowEpoch);
  if (out.today == 0) return false;
  const uint32_t mondayThisWeek = out.today - weekdayOfDay(out.today);
  out.startDay = mondayThisWeek - (HISTORY_WEEKS - 1) * 7;

  HalFile f = Storage.open(PATH, O_RDONLY);
  if (!f) return false;
  uint8_t header[HEADER_BYTES];
  if (!readHeader(f, header)) return false;
  out.sessions = get32(header + 4);
  out.totalSeconds = get32(header + 8);
  for (int h = 0; h < 24; ++h) out.hourSeconds[h] = get32(header + 12 + 4 * h);

  // ponytail: records are never pruned (about 2 KB a year); load() reads only the tail it needs.
  // Records are sorted by day and a record exists only for a day read, so the
  // last HISTORY_DAYS records always cover the whole window.
  const size_t count = (f.size() - HEADER_BYTES) / RECORD_BYTES;
  const size_t first = count > HISTORY_DAYS ? count - HISTORY_DAYS : 0;
  if (!f.seek(HEADER_BYTES + first * RECORD_BYTES)) return false;
  uint8_t record[RECORD_BYTES];
  for (size_t i = first; i < count; ++i) {
    if (f.read(record, RECORD_BYTES) != static_cast<int>(RECORD_BYTES)) break;
    const uint32_t day = get32(record);
    if (day < out.startDay || day > out.today) continue;
    out.seconds[day - out.startDay] = static_cast<uint16_t>(record[4] | record[5] << 8);
  }
  out.valid = true;
  return true;
}

bool loadYear(const int64_t nowEpoch, const int requestedYear, YearSummary& out) {
  out = YearSummary();
  const uint32_t today = localDay(nowEpoch);
  if (today == 0) return false;
  int month, dayOfMonth;
  civilFromDay(today, out.lastYear, month, dayOfMonth);
  out.firstYear = out.lastYear;
  out.year = requestedYear == 0 ? out.lastYear : requestedYear;
  const bool leap = (out.year % 4 == 0 && out.year % 100 != 0) || out.year % 400 == 0;
  out.days = leap ? 366 : 365;
  out.jan1 = dayFromCivil(out.year, 1, 1);

  HalFile f = Storage.open(PATH, O_RDONLY);
  uint8_t header[HEADER_BYTES];
  if (!f || !readHeader(f, header)) return false;
  const size_t count = (f.size() - HEADER_BYTES) / RECORD_BYTES;
  // Sixteen records per read keeps a multi-year log to a few SD reads.
  uint8_t chunk[16 * RECORD_BYTES];
  for (size_t i = 0; i < count;) {
    const size_t n = std::min<size_t>(16, count - i);
    if (f.read(chunk, n * RECORD_BYTES) != static_cast<int>(n * RECORD_BYTES)) break;
    for (size_t k = 0; k < n; ++k) {
      const uint8_t* record = chunk + k * RECORD_BYTES;
      const uint32_t day = get32(record);
      if (i == 0 && k == 0) {
        int y, m, d;
        civilFromDay(day, y, m, d);
        out.firstYear = std::min(y, out.lastYear);
      }
      if (day >= out.jan1 && day < out.jan1 + out.days) {
        out.seconds[day - out.jan1] = static_cast<uint16_t>(record[4] | record[5] << 8);
      }
    }
    i += n;
  }
  out.valid = true;
  return true;
}

}  // namespace readinglog
