#include "ButtonInput.h"

ButtonInput::ButtonInput(uint8_t pin, unsigned long debounceDelay, unsigned long doubleClickDelay)
  : _pin(pin),
    _debounceDelay(debounceDelay),
    _doubleClickDelay(doubleClickDelay),
    _lastRawState(HIGH),
    _stableState(HIGH),
    _lastChangeTime(0),
    _lastClickTime(0),
    _waitingForSecondClick(false),
    _singleClickFlag(false),
    _doubleClickFlag(false) {}

void ButtonInput::begin() {
  pinMode(_pin, INPUT_PULLUP);
  _lastRawState = digitalRead(_pin);
  _stableState = _lastRawState;
  _lastChangeTime = millis();
}

void ButtonInput::update() {
  bool currentRaw = digitalRead(_pin);
  unsigned long now = millis();

  if (currentRaw != _lastRawState) {
    _lastChangeTime = now;
    _lastRawState = currentRaw;
  }

  if ((now - _lastChangeTime) > _debounceDelay) {
    if (_stableState != currentRaw) {
      _stableState = currentRaw;

      if (_stableState == LOW) { // Button just pressed
        if (_waitingForSecondClick && (now - _lastClickTime) <= _doubleClickDelay) {
          _doubleClickFlag = true;
          _waitingForSecondClick = false;
        } else {
          _waitingForSecondClick = true;
          _lastClickTime = now;
        }
      }
    }
  }

  // Single click timeout check
  if (_waitingForSecondClick && (now - _lastClickTime) > _doubleClickDelay) {
    _singleClickFlag = true;
    _waitingForSecondClick = false;
  }
}

bool ButtonInput::SingleClick() {
  if (_singleClickFlag) {
    _singleClickFlag = false;
    return true;
  }
  return false;
}

bool ButtonInput::DoubleClick() {
  if (_doubleClickFlag) {
    _doubleClickFlag = false;
    return true;
  }
  return false;
}

bool ButtonInput::IsReallyPressed() {
  const unsigned long confirmDelay = 50; // milliseconds
  if (digitalRead(_pin) == LOW) {
    delay(confirmDelay);
    return digitalRead(_pin) == LOW;
  }
  return false;
}
