#pragma once
#include <Arduino.h>
#include "AppTypes.h"

class UserManager {
public:
  UserProfile* identify(float weightKg, UserProfile* profiles,
                        int count, float toleranceKg) {
    UserProfile* best     = nullptr;
    float        bestDiff = toleranceKg + 1.0f;

    for (int i = 0; i < count; i++) {
      float diff = fabsf(weightKg - profiles[i].weightKg);
      if (diff < bestDiff) {
        bestDiff = diff;
        best     = &profiles[i];
      }
    }
    if (best != nullptr && bestDiff <= toleranceKg) return best;
    return nullptr;
  }
};
