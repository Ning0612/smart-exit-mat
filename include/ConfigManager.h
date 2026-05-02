#pragma once
#include <Preferences.h>
#include "AppTypes.h"

class ConfigManager {
public:
  void load(AppConfig& cfg, UserProfile& user) {
    Preferences p;

    p.begin("cfg", true);
    cfg.wifiSsid               = p.getString("wifi_ssid",  "");
    cfg.wifiPassword           = p.getString("wifi_pass",  "");
    cfg.lineChannelAccessToken = p.getString("line_token", "");
    cfg.lineToId               = p.getString("line_to",    "");
    cfg.timezone               = p.getString("tz",         "CST-8");
    cfg.ntpServer              = p.getString("ntp",        "time.google.com");
    cfg.calibrationFactor      = p.getFloat ("cal_factor", 2280.0f);
    cfg.stepOnThresholdKg      = p.getFloat ("step_on",    10.0f);
    cfg.stepOffThresholdKg     = p.getFloat ("step_off",    3.0f);
    cfg.matchToleranceKg       = p.getFloat ("match_tol",   3.0f);
    cfg.cooldownMs             = p.getULong ("cooldown",  1500UL);
    p.end();

    p.begin("users", true);
    user.id       = "user1";
    user.name     = p.getString("user1_name",    "User1");
    user.weightKg = p.getFloat ("user1_weight",  60.0f);
    p.end();

    p.begin("state", true);
    user.atHome = p.getBool("user1_home", false);
    p.end();
  }

  void saveConfig(const AppConfig& cfg) {
    Preferences p;
    p.begin("cfg", false);
    p.putString("wifi_ssid",  cfg.wifiSsid);
    p.putString("wifi_pass",  cfg.wifiPassword);
    p.putString("line_token", cfg.lineChannelAccessToken);
    p.putString("line_to",    cfg.lineToId);
    p.putString("tz",         cfg.timezone);
    p.putString("ntp",        cfg.ntpServer);
    p.putFloat ("cal_factor", cfg.calibrationFactor);
    p.putFloat ("step_on",    cfg.stepOnThresholdKg);
    p.putFloat ("step_off",   cfg.stepOffThresholdKg);
    p.putFloat ("match_tol",  cfg.matchToleranceKg);
    p.putULong ("cooldown",   cfg.cooldownMs);
    p.end();
  }

  void saveUser(const UserProfile& user) {
    Preferences p;
    p.begin("users", false);
    p.putString("user1_name",   user.name);
    p.putFloat ("user1_weight", user.weightKg);
    p.end();
  }

  void saveUserState(const UserProfile& user) {
    Preferences p;
    p.begin("state", false);
    p.putBool("user1_home", user.atHome);
    p.end();
  }
};
