#pragma once
#include <Arduino.h>

enum class StepState { IDLE, MEASURING, COOLDOWN };

class StepDetector {
  StepState     _state          = StepState::IDLE;
  float         _eventMaxWeight = 0.0f;
  unsigned long _cooldownStart  = 0;

public:
  bool update(float weightKg, float stepOn, float stepOff, unsigned long cooldownMs) {
    switch (_state) {
      case StepState::IDLE:
        if (weightKg > stepOn) {
          _state          = StepState::MEASURING;
          _eventMaxWeight = weightKg;
        }
        break;

      case StepState::MEASURING:
        if (weightKg > _eventMaxWeight) _eventMaxWeight = weightKg;
        if (weightKg < stepOff) {
          _state        = StepState::COOLDOWN;
          _cooldownStart = millis();
          return true;
        }
        break;

      case StepState::COOLDOWN:
        if (millis() - _cooldownStart > cooldownMs) {
          _eventMaxWeight = 0.0f;
          _state          = StepState::IDLE;
        }
        break;
    }
    return false;
  }

  float     eventMaxWeightKg() const { return _eventMaxWeight; }
  StepState state()            const { return _state; }
};
