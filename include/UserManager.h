#pragma once
#include <Arduino.h>
#include "AppTypes.h"
#include "ConfigManager.h"

class UserManager {
  int   _adaptCount[MAX_USERS] = {};
  float _adaptSum  [MAX_USERS] = {};
  float _lastMargin             = 99.0f;

public:
  UserProfile* identify(float weightKg, UserProfile* profiles,
                        int count, float toleranceKg) {
    UserProfile* best           = nullptr;
    float        bestDiff       = toleranceKg + 1.0f;
    float        secondBestDiff = toleranceKg + 2.0f;
    _lastMargin = 99.0f;

    for (int i = 0; i < count; i++) {
      float diff = fabsf(weightKg - profiles[i].weightKg);
      if (diff < bestDiff) {
        secondBestDiff = bestDiff;
        bestDiff       = diff;
        best           = &profiles[i];
      } else if (diff < secondBestDiff) {
        secondBestDiff = diff;
      }
    }
    if (best != nullptr && bestDiff <= toleranceKg) {
      _lastMargin = secondBestDiff - bestDiff;
      return best;
    }
    return nullptr;
  }

  // 必須在 identify() 成功後、toggle() 之後呼叫（依賴 _lastMargin 為本次辨識結果）。
  // 連續 3 次偵測到一致偏差（> toleranceKg * 0.5）才更新，
  // 並在辨識不明確或讀值不一致時自動重置保護。
  bool adaptWeight(UserProfile* matched, float eventWeight,
                   float toleranceKg, ConfigManager& cfgMgr) {
    if (matched == nullptr) return false;
    int idx = matched->id.toInt() - 1;
    if (idx < 0 || idx >= MAX_USERS) return false;

    // 辨識不明確時跳過：另一使用者與最佳差距 < 1.5kg
    if (_lastMargin < 1.5f) return false;

    float diff               = fabsf(eventWeight - matched->weightKg);
    float deviationThreshold = toleranceKg * 0.5f;

    if (diff <= deviationThreshold) {
      // 體重吻合，重置累積器
      _adaptCount[idx] = 0;
      _adaptSum  [idx] = 0.0f;
      return false;
    }

    // 一致性驗證：新讀值須與累積平均值接近，否則重置
    if (_adaptCount[idx] > 0) {
      float currentAvg = _adaptSum[idx] / _adaptCount[idx];
      if (fabsf(eventWeight - currentAvg) > toleranceKg) {
        _adaptCount[idx] = 1;
        _adaptSum  [idx] = eventWeight;
        return false;
      }
    }

    _adaptCount[idx]++;
    _adaptSum  [idx] += eventWeight;

    if (_adaptCount[idx] >= 3) {
      float oldWeight = matched->weightKg;
      float newWeight = _adaptSum[idx] / _adaptCount[idx];
      _adaptCount[idx] = 0;
      _adaptSum  [idx] = 0.0f;

      if (newWeight >= 20.0f && newWeight <= 200.0f) {
        matched->weightKg = newWeight;
        cfgMgr.saveUser(*matched);
        Serial.printf("[WeightAdapt] %s %.1f->%.1f kg\n",
                      matched->name.c_str(), oldWeight, newWeight);
        return true;
      }
    }
    return false;
  }
};
