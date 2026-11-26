#pragma once

#include <cstddef>
#include <cstdint>

#include "ReadingLog.h"

class GfxRenderer;

// Drawing helpers shared by the Reading Activity sleep screen and stats page.
namespace readingview {

constexpr int GRID_CELL = 25;

// "23 min", "1 h", "2 h 10 min".
void formatDuration(uint32_t seconds, char* buf, size_t size);
// "9-11 pm" for the busiest two-hour window, empty when there is too little data.
void formatPeakHours(const readinglog::Summary& summary, char* buf, size_t size);
// "Night owl" and the like, from the busiest two hours; nullptr with too little data.
const char* readerType(const readinglog::Summary& summary);
// One line about the reading so far, a different one each day; empty when there is none.
void formatDailyFact(const readinglog::Summary& summary, char* buf, size_t size);

// One day: shaded by level (0 blank, 1 light, 2 medium, 3 black) with an outline.
void drawDayCell(const GfxRenderer& renderer, int x, int y, int size, uint8_t level);

// 18 weeks, one column per week (Monday on top), shaded relative to the reader's
// typical day. Month names sit above the columns that start a month.
void drawGrid(const GfxRenderer& renderer, int left, int top, const readinglog::Summary& summary, int cell = GRID_CELL);
// "Less [] [] [] [] More", right-aligned at right.
void drawShadeKey(const GfxRenderer& renderer, int right, int y, int cell);

// A 24-hour dial: midnight on top, one spoke per hour, as long as the time read in it.
void drawHourDial(const GfxRenderer& renderer, int cx, int cy, int innerRadius, int outerRadius,
                  const readinglog::Summary& summary);

// This week, Monday to Sunday: one bar per day with its minutes above it, today in black.
void drawWeekBars(const GfxRenderer& renderer, int left, int top, int width, int barHeight,
                  const readinglog::Summary& summary);

// Seconds read this week (Monday to today).
uint32_t thisWeekSeconds(const readinglog::Summary& summary);

}  // namespace readingview
