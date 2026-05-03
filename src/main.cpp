#include <Arduino.h>
#include <WiFi.h>

#define FORCE_CONFIG_PIN     0       // GPIO0 = BOOT button（低電位觸發）
#define FORCE_CONFIG_HOLD_MS 3000UL  // 長按 3 秒清空 WiFi 並重啟進 AP 模式

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
UserProfile  g_users[MAX_USERS];
int          g_userCount = 0;

ConfigManager g_configMgr;
ScaleManager  g_scale;
StepDetector  g_stepDetector;
UserManager   g_userMgr;
StateManager  g_stateMgr;
TimeManager   g_timeMgr;
LineNotifier  g_lineNotifier;
ConfigPortal  g_portal;

bool g_configMode = false;

// 清空 WiFi 憑證後重啟，讓下次開機因 SSID 為空而自動進入 AP 模式
// 呼叫前須確保 GPIO0 已放開，避免 bootloader strapping pin 進入 download mode
void clearWifiAndRestart() {
  g_cfg.wifiSsid = "";
  g_cfg.wifiPassword = "";
  g_configMgr.saveConfig(g_cfg);
  Serial.println("[Config] WiFi credentials cleared — restarting into AP mode...");
  delay(100);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[Boot] SmartExitMat starting");

  pinMode(FORCE_CONFIG_PIN, INPUT_PULLUP);

  g_configMgr.load(g_cfg, g_users, g_userCount);
  g_stateMgr.init(g_configMgr);
  g_scale.begin(g_cfg.calibrationFactor);

  if (g_cfg.wifiSsid.isEmpty()) {
    Serial.println("[Boot] No WiFi SSID — entering AP mode");
    g_configMode = true;
  } else {
    Serial.printf("[WiFi] Connecting to %s\n", g_cfg.wifiSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(g_cfg.wifiSsid.c_str(), g_cfg.wifiPassword.c_str());

    unsigned long t0 = millis();
    unsigned long gpio0HoldStart = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000UL) {
      delay(250);
      Serial.print(".");

      // WiFi 連線等待期間也監聽 GPIO0 長按
      if (digitalRead(FORCE_CONFIG_PIN) == LOW) {
        if (gpio0HoldStart == 0) gpio0HoldStart = millis();
        if (millis() - gpio0HoldStart >= FORCE_CONFIG_HOLD_MS) {
          Serial.println();
          Serial.println("[Boot] GPIO0 long press — release button to clear WiFi");
          WiFi.disconnect(true);
          while (digitalRead(FORCE_CONFIG_PIN) == LOW) delay(10);
          delay(100);
          clearWifiAndRestart();
        }
      } else {
        gpio0HoldStart = 0;
      }
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WiFi] Connected, IP: ");
      Serial.println(WiFi.localIP());
      g_timeMgr.begin(g_cfg.timezone, g_cfg.ntpServer);
    } else {
      Serial.println("[WiFi] Failed — entering AP mode");
      g_configMode = true;
    }
  }

  if (g_configMode) {
    g_portal.begin(g_cfg, g_users, g_userCount, g_configMgr);
  }
}

void loop() {
  // GPIO0 長按偵測：偵測到 3 秒後提示放開，放開後清空 WiFi 憑證並重啟
  // 等待放開才重啟，確保 bootloader 不會因 GPIO0 LOW 進入 download mode
  if (!g_configMode) {
    static unsigned long s_gpio0PressMs = 0;
    if (digitalRead(FORCE_CONFIG_PIN) == LOW) {
      if (s_gpio0PressMs == 0) s_gpio0PressMs = millis();
      if (millis() - s_gpio0PressMs >= FORCE_CONFIG_HOLD_MS) {
        Serial.println("[Runtime] GPIO0 long press — release button to clear WiFi and enter AP mode");
        while (digitalRead(FORCE_CONFIG_PIN) == LOW) delay(10);
        delay(100);
        clearWifiAndRestart();
      }
    } else {
      s_gpio0PressMs = 0;
    }
  }

  if (g_configMode) {
    g_portal.handleClient();
    return;
  }

  // HX711 未就緒時不更新狀態機，避免 0.0f 被誤判為離開事件
  if (!g_scale.isReady()) {
    g_timeMgr.updateDailySync(g_cfg.timezone, g_cfg.ntpServer);
    return;
  }

  float weight = g_scale.readWeightKg(3);  // 3 筆 ≈ 300ms，提升 GPIO0 採樣率
  Serial.printf("[Weight] %.2f kg\n", weight);  // TODO: remove after calibration

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
