#pragma once

#include <GfxRenderer.h>

#include <cstdint>

#include "CrossPointSettings.h"

// Font for definition and Ask AI text. It follows the reader font family but stops growing at
// MAX_POINT_SIZE, so a large book size does not turn a lookup into a screenful of giant words.
namespace dictionaryfont {

// ponytail: cap dictionary text at 18 pt; raise it if lookups read too small.
constexpr uint8_t MAX_POINT_SIZE = 18;

inline int fontId(const GfxRenderer& renderer) {
  const int readerFont = SETTINGS.getReaderFontId();
  const uint8_t size = SETTINGS.fontPointSize;
  if (size <= MAX_POINT_SIZE) return readerFont;
  // 0 when the capped size is unavailable (tight heap, family without it): keep the reader size.
  const int capped = renderer.resolveFontVariant(readerFont, (MAX_POINT_SIZE + 0.01f) / size);  // a hair over, so float rounding cannot pick the size below
  return capped != 0 ? capped : readerFont;
}

}  // namespace dictionaryfont
