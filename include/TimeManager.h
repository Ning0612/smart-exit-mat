#pragma once
#include <Arduino.h>
#include <time.h>

class TimeManager {
  bool          _synced     = false;
  unsigned long _lastSyncMs = 0;
  static const unsigned long SYNC_INTERVAL_MS  = 24UL * 3600UL * 1000UL;
  static const unsigned long RETRY_INTERVAL_MS =        5UL * 60UL * 1000UL;

public:
  // 開機時呼叫：允許阻塞 3 秒等待 NTP
  void begin(const String& timezone, const String& ntpServer) {
    configTzTime(timezone.c_str(), ntpServer.c_str());
    struct tm ti;
    if (getLocalTime(&ti, 3000)) {
      _synced     = true;
      _lastSyncMs = millis();
      Serial.println("[Time] NTP synced");
    } else {
      // 失敗時設定為 5 分鐘後重試（而非等 24 小時）
      _lastSyncMs = millis() - SYNC_INTERVAL_MS + RETRY_INTERVAL_MS;
      Serial.println("[Time] NTP sync failed, retry in 5 min");
    }
  }

  // loop() 中呼叫：非阻塞，僅觸發背景 NTP 更新
  void updateDailySync(const String& timezone, const String& ntpServer) {
    if (millis() - _lastSyncMs >= SYNC_INTERVAL_MS) {
      configTzTime(timezone.c_str(), ntpServer.c_str());
      _lastSyncMs = millis();
      // 確認時間是否已更新（非阻塞，timeout=0）
      struct tm ti;
      if (getLocalTime(&ti, 0)) {
        _synced = true;
        Serial.println("[Time] daily NTP refresh OK");
      } else {
        _lastSyncMs = millis() - SYNC_INTERVAL_MS + RETRY_INTERVAL_MS;
        Serial.println("[Time] daily NTP refresh pending, retry in 5 min");
      }
    }
  }

  String getCurrentTimeString() {
    struct tm ti;
    if (!getLocalTime(&ti, 0)) return String("(time not synced)");
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &ti);
    return String(buf);
  }

  bool isSynced() const { return _synced; }
};
