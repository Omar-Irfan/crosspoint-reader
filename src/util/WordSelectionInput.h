#pragma once

#include <cstdint>

#include "MappedInputManager.h"

class WordSelectionInput {
 public:
  enum ButtonEvent : uint8_t {
    INPUT_LEFT = 1,
    INPUT_RIGHT = 2,
    INPUT_UP = 4,
    INPUT_DOWN = 8,
    INPUT_CONFIRM = 16,
    INPUT_BACK = 32,
    INPUT_PREVIOUS = 64,
    INPUT_NEXT = 128
  };
  // Holding a direction moves once, then again after the delay, then at the rate.
  static constexpr uint32_t BUTTON_REPEAT_DELAY_MS = 400;
  static constexpr uint32_t BUTTON_REPEAT_MS = 150;

  static uint8_t buttonEdges(const MappedInputManager& input) {
    using B = MappedInputManager::Button;
    return (input.wasPressed(B::ScreenLeft) ? INPUT_LEFT : 0) | (input.wasPressed(B::ScreenRight) ? INPUT_RIGHT : 0) |
           (input.wasPressed(B::ScreenUp) ? INPUT_UP : 0) | (input.wasPressed(B::ScreenDown) ? INPUT_DOWN : 0) |
           (input.wasReleased(B::Confirm) ? INPUT_CONFIRM : 0) | (input.wasReleased(B::Back) ? INPUT_BACK : 0) |
           (input.wasPressed(B::NavPrevious) ? INPUT_PREVIOUS : 0) | (input.wasPressed(B::NavNext) ? INPUT_NEXT : 0);
  }

  uint8_t pollButtons(const MappedInputManager& input, const uint32_t now) {
    using B = MappedInputManager::Button;
    uint8_t buttons = buttonEdges(input);
    const uint8_t held =
        (input.isPressed(B::ScreenLeft) ? INPUT_LEFT : 0) | (input.isPressed(B::ScreenRight) ? INPUT_RIGHT : 0) |
        (input.isPressed(B::ScreenUp) ? INPUT_UP : 0) | (input.isPressed(B::ScreenDown) ? INPUT_DOWN : 0);
    constexpr uint8_t DIRECTIONS = INPUT_LEFT | INPUT_RIGHT | INPUT_UP | INPUT_DOWN;
    const bool single = held == INPUT_LEFT || held == INPUT_RIGHT || held == INPUT_UP || held == INPUT_DOWN;
    if (held != repeatingButton || (buttons & DIRECTIONS)) {
      repeatingButton = held;
      lastRepeat = now;
      repeating = false;
    } else if (single && now - lastRepeat >= (repeating ? BUTTON_REPEAT_MS : BUTTON_REPEAT_DELAY_MS)) {
      buttons |= held;
      lastRepeat = now;
      repeating = true;
    }
    return buttons;
  }

 private:
  uint8_t repeatingButton = 0;
  uint32_t lastRepeat = 0;
  bool repeating = false;
};
