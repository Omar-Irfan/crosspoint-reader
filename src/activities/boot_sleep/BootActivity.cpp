#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/BadrLabsLogo.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  drawBadrLabsLogo(renderer);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
