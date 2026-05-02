#include <Arduino.h>
#include <WiFi.h>

#define FORCE_CONFIG_PIN     0       // GPIO0 = BOOT button（低電位觸發）
#define FORCE_CONFIG_HOLD_MS 3000UL  // 長按 3 秒進入設定模式

#include "AppTypes.h"
#include "ConfigManager.h"
#include "ScaleManager.h"
#include "StepDetector.h"
#include "UserManager.h"
#include "StateManager.h"
#include "TimeManager.h"
#include "LineNotifier.h"
#include "ConfigPortal.h"

AppConfig    g_cfg;
UserProfile  g_users[1];
int          g_userCount = 1;

ConfigManager g_configMgr;
ScaleManager  g_scale;
StepDetector  g_stepDetector;
UserManager   g_userMgr;
StateManager  g_stateMgr;
TimeManager   g_timeMgr;
LineNotifier  g_lineNotifier;
ConfigPortal  g_portal;

bool g_configMode = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[Boot] SmartExitMat starting");

  // 長按 GPIO0 (BOOT) 3 秒強制進入設定模式
  bool forceConfig = false;
  pinMode(FORCE_CONFIG_PIN, INPUT_PULLUP);
  if (digitalRead(FORCE_CONFIG_PIN) == LOW) {
    Serial.println("[Boot] GPIO0 held — hold 3s to force config mode...");
    unsigned long t0 = millis();
    while (digitalRead(FORCE_CONFIG_PIN) == LOW) {
      if (millis() - t0 >= FORCE_CONFIG_HOLD_MS) {
        forceConfig = true;
        Serial.println("[Boot] Config mode forced by GPIO0 long press");
        break;
      }
      delay(50);
    }
  }

  g_configMgr.load(g_cfg, g_users[0]);
  g_stateMgr.init(g_configMgr);
  g_scale.begin(g_cfg.calibrationFactor);

  if (forceConfig || g_cfg.wifiSsid.isEmpty()) {
    Serial.println("[Boot] No WiFi SSID or forced — entering config mode");
    g_configMode = true;
  } else {
    Serial.printf("[WiFi] Connecting to %s\n", g_cfg.wifiSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_cfg.wifiSsid.c_str(), g_cfg.wifiPassword.c_str());

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000UL) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WiFi] Connected, IP: ");
      Serial.println(WiFi.localIP());
      g_timeMgr.begin(g_cfg.timezone, g_cfg.ntpServer);
    } else {
      Serial.println("[WiFi] Failed — entering config mode");
      g_configMode = true;
    }
  }

  if (g_configMode) {
    g_portal.begin(g_cfg, g_users[0], g_configMgr);
  }
}

void loop() {
  if (g_configMode) {
    g_portal.handleClient();
    return;
  }

  // HX711 未就緒時不更新狀態機，避免 0.0f 被誤判為離開事件
  if (!g_scale.isReady()) {
    g_timeMgr.updateDailySync(g_cfg.timezone, g_cfg.ntpServer);
    return;
  }

  float weight = g_scale.readWeightKg();

  bool eventFired = g_stepDetector.update(
    weight,
    g_cfg.stepOnThresholdKg,
    g_cfg.stepOffThresholdKg,
    g_cfg.cooldownMs
  );

  if (eventFired) {
    float eventWeight = g_stepDetector.eventMaxWeightKg();
    Serial.printf("[Step] event weight=%.1f kg\n", eventWeight);

    UserProfile* matched = g_userMgr.identify(
      eventWeight, g_users, g_userCount, g_cfg.matchToleranceKg
    );

    if (matched == nullptr) {
      Serial.println("[Step] unknown user — no action");
    } else {
      String eventType = g_stateMgr.toggle(*matched);
      String timestamp = g_timeMgr.getCurrentTimeString();

      Serial.printf("[State] %s %s @ %s (%.1f kg)\n",
        matched->name.c_str(), eventType.c_str(),
        timestamp.c_str(), eventWeight);

      g_lineNotifier.send(
        g_cfg.lineChannelAccessToken,
        g_cfg.lineToId,
        matched->name,
        eventType,
        timestamp,
        eventWeight
      );
    }
  }

  g_timeMgr.updateDailySync(g_cfg.timezone, g_cfg.ntpServer);
}
