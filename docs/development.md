# 開發指南

## 開發環境建置

### 必要工具

| 工具 | 版本要求 | 用途 |
|------|---------|------|
| [Visual Studio Code](https://code.visualstudio.com/) | 最新版 | 主要 IDE |
| [PlatformIO IDE 擴充套件](https://platformio.org/install/ide?install=vscode) | 最新版 | ESP32 構建系統 |
| Git | 任意版本 | 原始碼管理 |
| Python 3 | 3.6+ | PlatformIO 運行依賴（通常自動安裝） |

### 安裝步驟

1. 安裝 VS Code
2. 在 VS Code 擴充功能市集搜尋「PlatformIO IDE」並安裝
3. 重啟 VS Code，等待 PlatformIO 初始化（首次約 3–5 分鐘，需下載工具鏈）
4. 複製專案：

```bash
git clone https://github.com/Ning0612/smart-exit-mat.git
cd smart-exit-mat
```

5. 在 VS Code 中開啟專案資料夾（File → Open Folder）
6. PlatformIO 會自動辨識 `platformio.ini` 並提供底部工具列

### platformio.ini 說明

```ini
[env:nodemcu-32s]
platform  = espressif32        ; ESP32 平台工具鏈
board     = nodemcu-32s        ; NodeMCU-32S 板型（包含腳位定義）
framework = arduino            ; 使用 Arduino 框架

monitor_speed = 115200         ; Serial 監控 baud rate

board_build.partitions = partitions.csv   ; 自訂分區表
board_build.filesystem = littlefs         ; 使用 LittleFS（非 SPIFFS）

lib_deps =
  bogde/HX711@^0.7.5           ; HX711 驅動（bogde 維護版）
  bblanchon/ArduinoJson@^7     ; JSON 序列化 / 反序列化
```

> **LittleFS vs SPIFFS**：`board_build.filesystem = littlefs` 告知 PlatformIO 在上傳檔案系統時使用 LittleFS。雖然分區名稱仍標示 `spiffs`（ESP-IDF 兼容性），但實際格式為 LittleFS。

---

## 專案結構

```
smart-exit-mat/
├── src/
│   └── main.cpp              主程式：全域物件、setup()、loop()
├── include/                  模組（全部 header-only class）
│   ├── AppTypes.h            核心資料結構定義
│   ├── ConfigManager.h       NVS Preferences 讀寫
│   ├── ScaleManager.h        HX711 驅動封裝
│   ├── StepDetector.h        踩踏事件狀態機
│   ├── UserManager.h         使用者識別 + 體重自學習
│   ├── StateManager.h        出門/回家狀態切換與持久化
│   ├── TimeManager.h         NTP 校時與時間格式化
│   ├── LineNotifier.h        LINE Messaging API 推播
│   ├── WeatherManager.h      OpenWeatherMap 天氣查詢
│   ├── EventLogger.h         LittleFS JSONL 事件持久化
│   └── ConfigPortal.h        Web 設定頁 + Dashboard + REST API
├── docs/                     詳細文件（含 security.md 資安防護說明）
├── platformio.ini            PlatformIO 專案設定
├── partitions.csv            自訂 Flash 分區表
├── CLAUDE.md                 Claude Code 開發指引
└── README.md                 專案總覽
```

---

## 模組架構詳解

### AppTypes.h — 資料結構

所有模組共用的資料型別集中定義於此，避免循環依賴。

```cpp
constexpr int MAX_USERS = 10;   // NVS 上限（設定頁 UI 限制 5 位）

struct UserProfile {
  String id;        // "1"~"10"，對應 Preferences key 的數字索引
  String name;      // 使用者顯示名稱
  float  weightKg;  // 已知體重（公斤）
  bool   atHome;    // 當前在家狀態
};

struct EventRecord {
  time_t ts;        // Unix timestamp（NTP 同步後才有意義）
  String uid;       // "0"=Unknown, "1"~"10"=已知使用者
  String name;      // 顯示名稱，Unknown 為 "Unknown"
  String evType;    // "out" / "home" / "unknown"
  float  kg;        // 本次踩踏偵測重量
};

struct AppConfig {
  String wifiSsid, wifiPassword;
  String lineChannelAccessToken, lineToId;
  String timezone;                          // POSIX 時區字串（如 CST-8）
  String ntpServer;                         // 預設：time.google.com
  String owmApiKey;                         // 留空停用天氣
  String owmCity;                           // 預設：Taipei
  float  calibrationFactor;                 // HX711 校正係數；NVS 首次讀取預設 2280.0f（ConfigManager），AppTypes.h 的結構初始值 28853.34f 在 load() 後被覆蓋而無效
  float  stepOnThresholdKg;                 // 踩踏觸發（預設：10.0）
  float  stepOffThresholdKg;                // 離開觸發（預設：3.0）
  float  matchToleranceKg;                  // 識別容差（預設：3.0）
  unsigned long cooldownMs;                 // 冷卻時間（預設：1500 ms）
};
```

### ScaleManager.h — HX711 驅動

```cpp
// GPIO 定義（硬編碼）
static const int HX711_DT  = 26;
static const int HX711_SCK = 25;

class ScaleManager {
public:
  void  begin(float calibrationFactor);  // 初始化並等待 tare（最多 3 秒）
  float readWeightKg(int samples = 3);   // 取 samples 筆平均後轉換為公斤
  bool  isReady()    const;              // HX711 是否已就緒
  double getRawValue(int samples = 5);   // 取原始 ADC 值（校正用）
};
```

`tare()` 在 `begin()` 中自動執行，以開機時的空秤讀值為零點基準。

### StepDetector.h — 踩踏事件狀態機

```
狀態轉移：

 ┌─────────────────────────────────────────────────────────────┐
 │                                                             │
 ▼                                                             │
IDLE ──[weight > stepOn]──→ MEASURING ──[weight < stepOff]──→ COOLDOWN
 ▲                              │                                │
 │                              │ 同時記錄 eventMaxWeight        │
 │                              │                                │
 └──────────────────────────────────[cooldownMs 已過]───────────┘

返回 true（事件觸發）僅在 MEASURING → COOLDOWN 轉換時發生一次。
eventMaxWeightKg() 回傳本次踩踏期間測得的最大重量。
```

**為何記錄最大值而非平均值**：踩踏過程中重量曲線是先增後減（踩下→移重心→離開），取最大值更能反映真實體重，並與使用者登錄體重做準確比對。

### UserManager.h — 使用者識別與自適應學習

#### identify() — 最近鄰配對

```cpp
UserProfile* identify(float weightKg, UserProfile* profiles,
                      int count, float toleranceKg);
```

演算法：
1. 計算偵測重量與每位使用者登錄體重的差值（絕對值）
2. 找出最小差值 `bestDiff` 及第二小差值 `secondBestDiff`
3. 若 `bestDiff > toleranceKg`，判定為未知使用者，回傳 `nullptr`
4. 否則回傳最佳配對，並記錄 `_lastMargin = secondBestDiff - bestDiff`

`_lastMargin` 用於 `adaptWeight()` 中的安全判斷，margin 過小表示識別不明確，應跳過體重更新。

#### adaptWeight() — 體重自適應更新

觸發條件（同時滿足）：
- 識別成功（非 nullptr）
- `_lastMargin >= 1.5 kg`（識別結果明確，與第二候選差距足夠）
- 偵測重量偏差 `> toleranceKg * 0.5`（確實有偏移）
- 連續 3 次偏差方向一致（避免單次異常觸發更新）
- 新體重在 20.0–200.0 kg 範圍內（合理性驗證）

每次學習後自動呼叫 `ConfigManager::saveUser()` 持久化至 NVS。

### WeatherManager.h — 天氣資訊

更新策略：
- 成功：每 30 分鐘重新 fetch 一次
- 失敗：5 分鐘後重試
- 僅在地墊閒置（`weight < stepOnThreshold`）時執行，避免阻塞踩踏偵測

**URL 編碼**：`owmCity` 透過 `_urlEncode()` 處理後放入 OWM query string。允許直通字元：`A-Z a-z 0-9 - . _`；空白及其他所有字元均 `%XX` 百分比編碼，防止城市名稱注入額外 query 參數。

天氣建議邏輯（`_buildCurrentAdvisory()`）：

| 條件 | 建議文字 |
|------|---------|
| 雷電（id 200–299） | 正在打雷閃電，請注意安全 |
| 毛毛雨（id 300–399） | 正在飄毛毛雨，記得帶傘 |
| 下雨（id 500–599） | 正在下雨，記得帶傘 |
| 下雪（id 600–699） | 正在下雪，注意保暖 |
| 氣溫 < 12°C | 天氣寒冷，記得穿外套保暖 |
| 氣溫 12–16°C | 天氣涼，建議多穿一件 |
| 體感 > 35°C | 天氣炎熱，注意防曬補水 |

12 小時預報（cnt=4，每 3h 一筆）另外判斷未來降雨機率（pop > 0.4），若有則附加預警。

### EventLogger.h — 事件日誌

#### 檔案格式

儲存路徑：`/events/YYYY-MM.jsonl`

每行一條 JSON 物件（JSONL 格式）：
```json
{"ts":1746345600,"uid":"1","nm":"Lance","ev":"out","kg":72.3}
```

欄位說明：
- `ts`：Unix timestamp（秒）
- `uid`：使用者 ID（"0" = Unknown）
- `nm`：使用者名稱
- `ev`：`"out"`（出門）/ `"home"`（回家）/ `"unknown"`（未知踩踏）
- `kg`：本次踩踏最大重量

#### 查詢範圍

`getEventsJson()` 支援三種視圖，依照時間區間讀取對應月份的 JSONL 檔：

| view | 時間區間 | 跨月處理 |
|------|---------|---------|
| `day` | 指定日期 00:00–23:59 | 不跨月 |
| `week` | 週一 00:00 到週日 23:59（7 天） | 可能跨月，自動合併兩個月的檔案 |
| `month` | 指定月份全月 | 單月 |

#### NTP 依賴

`log()` 在 `time(nullptr) < 1,000,000,000` 時（NTP 未同步）直接回傳 `false` 並捨棄記錄，不寫入假時間戳。`pruneOldFiles()` 同樣如此，需在 NTP 同步後呼叫才有效。

---

## NVS Preferences 命名空間

所有設定持久化使用 ESP32 的 NVS（Non-Volatile Storage），透過 `Preferences` 函式庫存取。**所有 key 長度不得超過 15 字元**（NVS 硬限制）。

### `cfg` 命名空間（設定）

| Key | 型別 | 說明 | 預設值 |
|-----|------|------|--------|
| `wifi_ssid` | String | WiFi SSID | `""` |
| `wifi_pass` | String | WiFi 密碼 | `""` |
| `line_token` | String | LINE Channel Access Token | `""` |
| `line_to` | String | LINE 接收者 ID | `""` |
| `tz` | String | POSIX 時區字串 | `"CST-8"` |
| `ntp` | String | NTP 伺服器 | `"time.google.com"` |
| `owm_key` | String | OpenWeatherMap API Key | `""` |
| `owm_city` | String | 城市名稱 | `"Taipei"` |
| `cal_factor` | float | HX711 校正係數 | `2280.0` |
| `step_on` | float | 踩踏觸發門檻 (kg) | `10.0` |
| `step_off` | float | 離開觸發門檻 (kg) | `3.0` |
| `match_tol` | float | 識別容差 (kg) | `3.0` |
| `cooldown` | ulong | 冷卻時間 (ms) | `1500` |

### `users` 命名空間（使用者資料）

| Key 格式 | 型別 | 說明 |
|---------|------|------|
| `user_count` | int | 已登錄使用者數量 |
| `userN_name` | String | 使用者 N 的名稱（N=1–10） |
| `userN_weight` | float | 使用者 N 的體重 (kg) |

### `state` 命名空間（即時狀態）

| Key 格式 | 型別 | 說明 |
|---------|------|------|
| `userN_home` | bool | 使用者 N 是否在家 |

---

## 新增功能注意事項

### 新增 AppConfig 欄位

需同步修改四個位置：
1. **`AppTypes.h`**：在 `AppConfig` 結構加入新欄位
2. **`ConfigManager.h`**：在 `load()` 加入 `p.getString/getFloat(...)` 讀取；在 `saveConfig()` 加入對應 `p.putXxx(...)` 寫入
3. **`ConfigPortal.h`**：在 HTML 表單中加入對應 `<input>` 欄位；在 `_handleSave()` 中解析並寫入 `_cfg`
4. 確認 Preferences key 長度 ≤ 15 字元

### 新增 ConfigPortal 路由

在 `_attachRoutes()` 中加入：
```cpp
_server->on("/api/new", HTTP_GET, [this]() { _handleApiNew(); });
```

若 handler 需要存取 `_weather`、`_eventLogger`、`_timeMgr` 等，必須先確認呼叫端已透過對應 setter 注入：
```cpp
g_portal.setWeatherManager(g_weather);
g_portal.setEventLogger(g_eventLogger);
```

### 主迴圈時序限制

- HTTPS 呼叫（LINE、OWM）具有阻塞性（~1–3 秒），**只能在地墊閒置時執行**
- 目前實作已限制：`if (weight < stepOnThresholdKg) { g_weather.updateIfNeeded(...); }`
- 若需新增外部 API 呼叫，同樣須加入此條件保護

---

## 常用開發指令

### CI

GitHub Actions 會在 push、pull request 與手動 workflow dispatch 時執行
`.github/workflows/ci.yml`：

- 安裝 PlatformIO
- 執行 `pio run -e nodemcu-32s`

CI 只驗證韌體可建置，不會燒錄裝置、連接 HX711、呼叫 LINE Bot 或
OpenWeatherMap，也不會執行實體踩踏測試。

```bash
# 僅編譯（驗證語法）
pio run

# 編譯 + 上傳
pio run --target upload

# Serial 監控（115200 baud，Ctrl+C 退出）
pio device monitor

# 清除編譯快取
pio run --target clean

# 全片 Flash 擦除（危險：清除所有資料）
pio run -t erase

# 列出可用設備
pio device list
```

> **Windows PowerShell 5.1**：不支援 `&&` 串聯指令，請改用 PowerShell 7+ 或 Git Bash，或分兩行執行。  
> **Windows pio 路徑**：若 `pio` 未在全域 PATH（Claude Code 工作階段），請使用完整路徑：
> ```powershell
> $pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
> & $pio run          # 編譯驗證
> & $pio run --target upload
> ```
