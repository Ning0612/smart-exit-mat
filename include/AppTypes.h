#pragma once
#include <Arduino.h>

constexpr int MAX_USERS = 10;

struct UserProfile {
  String id;
  String name;
  float  weightKg;
  bool   atHome;
};

struct EventRecord {
  time_t ts;       // Unix timestamp
  String uid;      // "1"~"10"
  String name;
  String evType;   // "out" | "home"
  float  kg;
};

struct AppConfig {
  String wifiSsid;
  String wifiPassword;
  String lineChannelAccessToken;
  String lineToId;
  String timezone;
  String ntpServer;
  String owmApiKey;
  String owmCity             = "Taipei";
  float  calibrationFactor  = 28853.34f;
  float  stepOnThresholdKg  = 10.0f;
  float  stepOffThresholdKg =  3.0f;
  float  matchToleranceKg   =  3.0f;
  unsigned long cooldownMs  = 1500UL;
};
