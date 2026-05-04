#pragma once
#include <Preferences.h>
#include "AppTypes.h"

class ConfigManager {
public:
  void load(AppConfig& cfg, UserProfile* users, int& userCount) {
    Preferences p;

    p.begin("cfg", true);
    cfg.wifiSsid               = p.getString("wifi_ssid",  "");
    cfg.wifiPassword           = p.getString("wifi_pass",  "");
    cfg.lineChannelAccessToken = p.getString("line_token", "");
    cfg.lineToId               = p.getString("line_to",    "");
    cfg.timezone               = p.getString("tz",         "CST-8");
    cfg.ntpServer              = p.getString("ntp",        "time.google.com");
    cfg.owmApiKey              = p.getString("owm_key",   "");
    cfg.owmCity                = p.getString("owm_city",  "Taipei");
    cfg.apPassword             = p.getString("ap_pass",    "12345678");
    cfg.adminPassword          = p.getString("admin_pass", "");
    cfg.calibrationFactor      = p.getFloat ("cal_factor", 2280.0f);
    cfg.stepOnThresholdKg      = p.getFloat ("step_on",    10.0f);
    cfg.stepOffThresholdKg     = p.getFloat ("step_off",    3.0f);
    cfg.matchToleranceKg       = p.getFloat ("match_tol",   3.0f);
    cfg.cooldownMs             = p.getULong ("cooldown",  1500UL);
    p.end();

    p.begin("users", true);
    int saved = p.getInt("user_count", 1);  // default 1 for backward compat
    if (saved < 0) saved = 0;
    if (saved > MAX_USERS) saved = MAX_USERS;
    userCount = saved;

    for (int i = 1; i <= userCount; i++) {
      String pfx = "user" + String(i);
      users[i-1].id       = String(i);
      users[i-1].name     = p.getString((pfx + "_name").c_str(),    "User" + String(i));
      users[i-1].weightKg = p.getFloat ((pfx + "_weight").c_str(),  60.0f);
    }
    p.end();

    p.begin("state", true);
    for (int i = 1; i <= userCount; i++) {
      users[i-1].atHome = p.getBool(("user" + String(i) + "_home").c_str(), false);
    }
    p.end();
  }

  void saveConfig(const AppConfig& cfg) {
    Preferences p;
    p.begin("cfg", false);
    p.putString("wifi_ssid",  cfg.wifiSsid);
    p.putString("wifi_pass",  cfg.wifiPassword);
    p.putString("line_token", cfg.lineChannelAccessToken);
    p.putString("line_to",    cfg.lineToId);
    p.putString("owm_key",    cfg.owmApiKey);
    p.putString("owm_city",   cfg.owmCity);
    p.putString("ap_pass",    cfg.apPassword);
    p.putString("admin_pass", cfg.adminPassword);
    p.putString("tz",         cfg.timezone);
    p.putString("ntp",        cfg.ntpServer);
    p.putFloat ("cal_factor", cfg.calibrationFactor);
    p.putFloat ("step_on",    cfg.stepOnThresholdKg);
    p.putFloat ("step_off",   cfg.stepOffThresholdKg);
    p.putFloat ("match_tol",  cfg.matchToleranceKg);
    p.putULong ("cooldown",   cfg.cooldownMs);
    p.end();
  }

  // user.id must be "1"~"5" (set during load or _handleSave)
  void saveUser(const UserProfile& user) {
    int idx = user.id.toInt();
    if (idx < 1 || idx > MAX_USERS) return;
    Preferences p;
    p.begin("users", false);
    String pfx = "user" + String(idx);
    p.putString((pfx + "_name").c_str(),   user.name);
    p.putFloat ((pfx + "_weight").c_str(), user.weightKg);
    p.end();
  }

  void saveUserState(const UserProfile& user) {
    int idx = user.id.toInt();
    if (idx < 1 || idx > MAX_USERS) return;
    Preferences p;
    p.begin("state", false);
    p.putBool(("user" + String(idx) + "_home").c_str(), user.atHome);
    p.end();
  }

  void saveUserCount(int count) {
    if (count < 0) count = 0;
    if (count > MAX_USERS) count = MAX_USERS;
    Preferences p;
    p.begin("users", false);
    p.putInt("user_count", count);
    p.end();
  }
};
