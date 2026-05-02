#pragma once
#include <Arduino.h>

struct UserProfile {
  String id;
  String name;
  float  weightKg;
  bool   atHome;
};

struct AppConfig {
  String wifiSsid;
  String wifiPassword;
  String lineChannelAccessToken;
  String lineToId;
  String timezone;
  String ntpServer;
  float  calibrationFactor  = 2280.0f;
  float  stepOnThresholdKg  = 10.0f;
  float  stepOffThresholdKg =  3.0f;
  float  matchToleranceKg   =  3.0f;
  unsigned long cooldownMs  = 1500UL;
};
