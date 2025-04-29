#ifndef BUTTON_INPUT_H
#define BUTTON_INPUT_H

#include <Arduino.h>

class ButtonInput {
public:
  ButtonInput(uint8_t pin, unsigned long debounceDelay = 60, unsigned long doubleClickDelay = 400);

  void begin();
  void update();

  bool SingleClick();
  bool DoubleClick();
  bool IsReallyPressed(); // ✅ Confirm the button is really pressed (not just bouncing)

private:
  uint8_t _pin;
  unsigned long _debounceDelay;
  unsigned long _doubleClickDelay;

  bool _lastRawState;
  bool _stableState;
  unsigned long _lastChangeTime;

  unsigned long _lastClickTime;
  bool _waitingForSecondClick;

  bool _singleClickFlag;
  bool _doubleClickFlag;
};

#endif
