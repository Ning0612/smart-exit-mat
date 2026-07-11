# 你出門了嗎？ 智慧地墊系統

[![GitHub](https://img.shields.io/badge/GitHub-Ning0612%2Fsmart--exit--mat-181717?logo=github)](https://github.com/Ning0612/smart-exit-mat)
![ESP32](https://img.shields.io/badge/ESP32-NodeMCU--32S-blue?logo=espressif)
![Arduino](https://img.shields.io/badge/Framework-Arduino-teal?logo=arduino)
![PlatformIO](https://img.shields.io/badge/Build-PlatformIO-orange?logo=platformio)
![C++](https://img.shields.io/badge/Language-C%2B%2B-blue?logo=cplusplus)
![ArduinoJson](https://img.shields.io/badge/Lib-ArduinoJson%20v7-blue)
![LittleFS](https://img.shields.io/badge/Storage-LittleFS-green)
![LINE Bot](https://img.shields.io/badge/Notify-LINE%20Bot-brightgreen?logo=line)
![OpenWeatherMap](https://img.shields.io/badge/Weather-OpenWeatherMap-orange)
![HX711](https://img.shields.io/badge/Sensor-HX711%20%2B%20Load%20Cell-red)
[![CI](https://github.com/Ning0612/smart-exit-mat/actions/workflows/ci.yml/badge.svg)](https://github.com/Ning0612/smart-exit-mat/actions/workflows/ci.yml)

利用踩踏地墊時的重量變化，自動判斷家庭成員出門或回家，並透過 **LINE Bot** 發送即時通知。出門時附上即時天氣建議，所有事件持久化至 LittleFS，提供 **Web 儀表板**查閱歷史記錄。

## 專案狀態

本專案是智慧地墊系統原型。公開整理已完成，後續不再持續維護。

---

## 功能概覽

- 踩踏偵測：三態狀態機（IDLE → MEASURING → COOLDOWN），門檻與冷卻時間可調
- 多使用者識別：最近體重配對，最多 5 位（NVS 最多 10 位）
- 自適應體重學習：連續 3 次一致偏差後自動更新登錄體重
- 出門／回家狀態交替切換，斷電後狀態保留（NVS 持久化）
- LINE Bot 通知：上線通知、踩踏通知（姓名、事件、時間、重量）、未知踩踏警示
- 天氣建議：出門時附當前天氣與 12 小時降雨預警（需 OpenWeatherMap API Key）
- 事件日誌：JSONL 格式按月儲存於 LittleFS，自動清理 6 個月前舊檔；支援透過 Dashboard 手動清除全部紀錄（需管理員密碼確認）
- Web 儀表板：即時狀態、日/週/月報表圖表、12 小時事件時間軸
- 雙 WiFi 模式：STA 常態運作 / AP 初始設定
- 硬體重置：長按 BOOT 鍵（GPIO0）3 秒後**放開**，清空 WiFi 憑證並重置 AP 密碼與管理密碼，重新進入 AP 模式

---

## 快速上手

### 第一步：硬體接線

NodeMCU-32S ← HX711 ← 四顆 50 kg Load Cell（惠斯通電橋）  
→ 詳見 [docs/hardware.md](docs/hardware.md)

### 第二步：燒錄韌體

```bash
git clone https://github.com/Ning0612/smart-exit-mat.git
cd smart-exit-mat
pio run -t erase          # 首次建議全片擦除
pio run --target upload   # 編譯並上傳
pio device monitor        # 確認啟動正常（115200 baud）
```

→ 詳見 [docs/deployment.md](docs/deployment.md)

### 第三步：初始設定

1. 連接 WiFi：**`SmartExitMat-Setup`**，WPA2 密碼：**`12345678`**（預設值，開機時顯示於 Serial）
2. 開啟外部瀏覽器（Chrome、Safari、Edge 等）：`http://192.168.4.1`
   > LINE App 內建瀏覽器不支援 HTTP Basic Auth 登入彈窗；設定管理員密碼後，請改用外部瀏覽器開啟設定頁。
3. **先在「安全設定」區塊修改 AP 密碼與管理員密碼**（建議在填入 LINE Token 等敏感資訊前完成）
4. 填入 WiFi、LINE Token、使用者體重，儲存重啟

→ 詳見 [docs/configuration.md](docs/configuration.md)

### 第四步：校正秤

站上地墊，呼叫 `http://<裝置IP>/api/calibrate?w=<體重kg>`，將 `factor` 填入設定頁。

→ 詳見 [docs/calibration.md](docs/calibration.md)

---

## 文件目錄

| 文件 | 內容 |
|------|------|
| [docs/hardware.md](docs/hardware.md) | 元件清單、GPIO 接線、Load Cell 橋接方式 |
| [docs/development.md](docs/development.md) | 開發環境、專案結構、模組架構、NVS 命名空間、擴充指引 |
| [docs/deployment.md](docs/deployment.md) | Flash 分區配置、首次燒錄流程、Serial 監控說明 |
| [docs/configuration.md](docs/configuration.md) | 所有設定欄位說明、LINE Bot 取得步驟、OWM API 設定 |
| [docs/usage.md](docs/usage.md) | 開機流程、踩踏狀態機、使用者識別、自學習、硬體重置 |
| [docs/api.md](docs/api.md) | 完整 REST API 參考（路由、請求、回應格式） |
| [docs/calibration.md](docs/calibration.md) | 秤校正原理、API 自動校正、手動計算方法 |
| [docs/troubleshooting.md](docs/troubleshooting.md) | 常見問題診斷與解決方法 |
| [docs/security.md](docs/security.md) | 資安防護說明（密碼管理、TLS 憑證驗證、CSRF 防護） |

---

## 系統架構

```
HX711 + Load Cell
       │
  ScaleManager
       │
  StepDetector ─── 踩踏事件
       │
  UserManager ──── 識別使用者（最近體重配對）
       │
  StateManager ─── 切換出門/回家 → NVS 持久化
       │
  ┌────┴────────┐
  │             │
LineNotifier  EventLogger
(LINE Bot)   (LittleFS JSONL)
       │
  WeatherManager（OWM API，閒置時更新）

ConfigPortal ─── Web 設定頁 + Dashboard + REST API
ConfigManager ── NVS Preferences 讀寫
TimeManager ──── NTP 校時（time.google.com）
```

---

## Demo 片段

- [smart-exit-mat 實際動作 demo](docs/demo/line-demo.mp4)

## 授權

本公開版本採 MIT License，詳見 [LICENSE](LICENSE)。
