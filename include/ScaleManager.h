#pragma once
#include <HX711.h>

#define HX711_DT_PIN  26
#define HX711_SCK_PIN 25

class ScaleManager {
  HX711 _scale;
public:
  void begin(float calibrationFactor) {
    _scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
    _scale.set_scale(calibrationFactor);
    // 等待 HX711 就緒再 tare，最多等 3 秒
    unsigned long t0 = millis();
    while (!_scale.is_ready() && millis() - t0 < 3000UL) delay(10);
    if (_scale.is_ready()) {
      _scale.tare(10);
      Serial.println("[Scale] tare done");
    } else {
      Serial.println("[Scale] HX711 not ready after 3s — tare skipped");
    }
  }

  bool isReady() { return _scale.is_ready(); }

  float readWeightKg(int samples = 10) {
    if (!_scale.is_ready()) return 0.0f;
    float raw = _scale.get_units(samples);
    return (raw < 0.0f) ? 0.0f : raw;
  }
};
