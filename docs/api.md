# API 參考文件

所有 API 皆由裝置本身提供，無需後端伺服器。存取前提：裝置與客戶端需在**同一區域網路**。

Base URL：`http://<裝置IP>`

---

## 頁面路由

### `GET /`

設定頁面，提供完整的 Web 設定介面。

**功能**：
- WiFi SSID / 密碼設定（含掃描按鈕，AP 模式下可用）
- LINE Channel Access Token 與接收者 ID
- OpenWeatherMap API Key 與城市
- 時區（POSIX 格式）與 NTP 伺服器
- 秤校正係數與門檻參數
- 使用者管理（新增/刪除，最多 5 位）

---

### `GET /dashboard`

儀表板頁面，提供即時狀態與事件報表視覺化。

**功能**：
- 即時天氣卡片（溫度、體感、建議）
- 使用者在家狀態卡片
- 事件報表（日 / 週 / 月，含圖表）
- 過去 12 小時狀態時間軸（甘特圖式）
- 清除所有事件紀錄（需輸入管理員密碼確認）

---

## REST API

### `GET /api/status`

查詢當前天氣與所有使用者的在家狀態。

**回應**：`Content-Type: application/json`

```json
{
  "weather": {
    "has_data": true,
    "temp_c": 25.3,
    "feels_like_c": 24.1,
    "advisory": "待會可能下雨（機率 60%），建議帶傘",
    "city": "Taipei"
  },
  "users": [
    { "id": "1", "name": "Lance", "at_home": true },
    { "id": "2", "name": "Alice", "at_home": false }
  ],
  "time": "14:30:25"
}
```

**欄位說明**：

`weather` 物件：

| 欄位 | 型別 | 說明 |
|------|------|------|
| `has_data` | boolean | 天氣資料是否可用（false = API Key 未設定或 fetch 失敗） |
| `temp_c` | number | 當前氣溫（°C），`has_data=false` 時不存在 |
| `feels_like_c` | number | 體感溫度（°C） |
| `advisory` | string | 天氣建議文字（可能含換行 `\n`） |
| `city` | string | 城市名稱 |

`users` 陣列：

| 欄位 | 型別 | 說明 |
|------|------|------|
| `id` | string | 使用者 ID，"1"–"10" |
| `name` | string | 使用者名稱 |
| `at_home` | boolean | 是否在家 |

其他欄位：

| 欄位 | 型別 | 說明 |
|------|------|------|
| `time` | string | 裝置目前時間，格式 `YYYY-MM-DD HH:MM`（NTP 未同步時為字串 `"(time not synced)"`） |

---

### `GET /api/events`

查詢事件日誌，依視圖範圍（日/週/月）返回結果。

**Query 參數**：

| 參數 | 型別 | 必填 | 說明 |
|------|------|------|------|
| `view` | string | 否（預設 `day`） | `day` / `week` / `month` |
| `date` | string | `view=day/week` 時 | 格式 `YYYY-MM-DD`（week 傳週一日期） |
| `year` | int | `view=month` 時 | 年份（如 `2025`） |
| `month` | int | `view=month` 時 | 月份（1–12） |

> 若省略 `date`、`year`、`month`，則使用裝置目前時間對應的範圍。

**範例請求**：

```
GET /api/events?view=day&date=2025-05-04
GET /api/events?view=week&date=2025-04-28
GET /api/events?view=month&year=2025&month=5
```

**回應**：

```json
{
  "view": "day",
  "events": [
    {
      "ts": 1746345600,
      "uid": "1",
      "nm": "Lance",
      "ev": "out",
      "kg": 72.3
    },
    {
      "ts": 1746388200,
      "uid": "1",
      "nm": "Lance",
      "ev": "home",
      "kg": 71.9
    },
    {
      "ts": 1746390000,
      "uid": "0",
      "nm": "Unknown",
      "ev": "unknown",
      "kg": 45.2
    }
  ],
  "count": 3
}
```

**事件欄位說明**：

| 欄位 | 型別 | 說明 |
|------|------|------|
| `ts` | number | Unix timestamp（秒） |
| `uid` | string | 使用者 ID（`"0"` = 未知使用者） |
| `nm` | string | 使用者名稱（`"Unknown"` = 未知） |
| `ev` | string | 事件類型：`"out"` 出門 / `"home"` 回家 / `"unknown"` 未知踩踏 |
| `kg` | number | 本次踩踏偵測重量（kg） |

**限制**：每次最多回傳 300 筆事件（含跨月查詢合計）。

---

### `GET /api/events/status`

查詢使用者在過去 12 小時的出門/回家狀態時間軸，供儀表板甘特圖使用。

**Query 參數（均為選填）**：

| 參數 | 型別 | 說明 |
|------|------|------|
| `start` | int | 時間窗口起點的 Unix timestamp，必須在當前時間 ±7 天內。省略時使用「目前時間 - 12h」 |

**回應**：

```json
{
  "ws": 1746302400,
  "we": 1746345600,
  "users": [
    { "uid": "1", "nm": "Lance", "home": true },
    { "uid": "2", "nm": "Alice", "home": false }
  ],
  "events": [
    { "ts": 1746310000, "uid": "1", "nm": "Lance", "ev": "out", "kg": 72.1 },
    { "ts": 1746340000, "uid": "1", "nm": "Lance", "ev": "home", "kg": 72.5 }
  ]
}
```

| 欄位 | 說明 |
|------|------|
| `ws` | 時間窗口起點（Unix timestamp） |
| `we` | 時間窗口終點（`ws + 43200`，即 12 小時後） |
| `users` | 所有使用者當前狀態（含 `home` 布林值），供前端推算初始狀態 |
| `events` | 時間窗口內（實際查詢 `ws - 24h` 到 `we`）的所有事件，最多 300 筆 |

---

### `GET /api/csrf`

取得 CSRF nonce token，供 `POST /api/events/clear` 使用。

**認證**：需要 HTTP Basic Auth（與其他 API 一致）。

**回應**：

```json
{ "token": "<16-hex-csrf-token>" }
```

Token 為 per-boot 16 碼十六進位字串，裝置重啟後更換。

---

### `POST /api/events/clear`

**永久刪除** LittleFS 中 `/events/` 目錄下所有事件日誌檔案（`YYYY-MM.jsonl` 格式）。此操作不可復原。

**認證**：需要 HTTP Basic Auth。

**Content-Type**：`application/x-www-form-urlencoded`（或 `multipart/form-data`）

**欄位**：

| 欄位 | 必填 | 說明 |
|------|------|------|
| `_csrf` | 是 | 從 `GET /api/csrf` 取得的 token |
| `password` | 條件必填 | 管理員密碼；`adminPassword` 已設定時必填，未設定時可省略 |

**成功回應（HTTP 200）**：

```json
{ "ok": true }
```

**失敗回應**：

| HTTP 狀態碼 | 說明 | 回應範例 |
|------------|------|---------|
| 403 | CSRF token 不符 | `{"ok":false,"error":"CSRF token mismatch"}` |
| 403 | 管理員密碼錯誤 | `{"ok":false,"error":"密碼錯誤"}` |
| 429 | 密碼嘗試過多（5 次失敗後觸發 30s 冷卻） | `{"ok":false,"error":"too many attempts, wait 30s"}` |
| 503 | EventLogger 未就緒 | `{"ok":false,"error":"logger not ready"}` |

**注意事項**：
- 密碼比對失敗超過 **5 次**後，進入 30 秒冷卻期，期間所有嘗試均回傳 429
- 冷卻計數於裝置重啟後重置
- 只刪除符合 `YYYY-MM.jsonl` 命名格式的檔案；目錄結構與其他自訂檔案不受影響

---

### `GET /api/calibrate?w=<kg>`

利用目前 HX711 讀值自動計算校正係數。

**必要條件**：
- 呼叫時必須站在地墊上（或放置已知重量物體）
- 重量必須 ≥ 5 kg

**Query 參數**：

| 參數 | 型別 | 必填 | 說明 |
|------|------|------|------|
| `w` | float | 是 | 實際重量（kg），最小 5.0 |

**範例**：

```
GET /api/calibrate?w=72.5
```

**成功回應（HTTP 200）**：

```json
{
  "ok": true,
  "raw": 165420,
  "factor": 2281.66
}
```

| 欄位 | 說明 |
|------|------|
| `ok` | `true` = 校正成功 |
| `raw` | HX711 原始 ADC 讀值（5 次取樣平均） |
| `factor` | 計算得到的校正係數（= raw ÷ w）|

**失敗回應（HTTP 400 或 503）**：

```json
{ "ok": false, "error": "missing param w" }
{ "ok": false, "error": "weight must be >= 5 kg" }
{ "ok": false, "error": "no weight detected — please stand on the mat" }
{ "ok": false, "error": "wiring reversed? raw=-12345" }
{ "ok": false, "error": "scale not initialized" }
```

取得 `factor` 後，需手動進入設定頁將 Calibration Factor 更新為此值，再點選「儲存設定」。

---

### `GET /scan`

掃描附近 WiFi 網路，供設定頁 SSID 下拉選單使用。

**重要**：
- **AP 模式（初始設定）下**：執行實際 WiFi 掃描，有 5 秒冷卻限制（頻繁呼叫直接回傳 `[]`）
- **STA 模式（已連上 WiFi）下**：直接回傳空陣列 `[]`，不執行掃描

**回應（AP 模式掃描成功）**：

```json
["HomeWiFi", "NeighborNet", "Office_5G"]
```

回傳字串陣列，每個元素為 SSID，已去除重複（同 SSID 不同 BSSID 僅保留一個）。

---

### `POST /save`

儲存設定頁所有欄位並重啟裝置。

**Content-Type**：`application/x-www-form-urlencoded`（HTML 表單提交）

**欄位（均為選填，不傳的欄位不更新）**：

| 欄位名 | 類型 | 說明 |
|--------|------|------|
| `wifi_ssid` | string | WiFi SSID |
| `wifi_pass` | string | WiFi 密碼；**空值保留現有密碼** |
| `wifi_pass_clear` | checkbox（值 `"1"`） | 勾選時清除 WiFi 密碼（開放網路用） |
| `line_token` | string | LINE Channel Access Token；**空值保留現有 Token** |
| `line_token_clear` | checkbox（值 `"1"`） | 勾選時清除 Token（停用 LINE 通知） |
| `line_to` | string | LINE 接收者 ID；**空值保留現有值** |
| `line_to_clear` | checkbox（值 `"1"`） | 勾選時清除 User ID |
| `owm_key` | string | OWM API Key；**空值保留現有 Key** |
| `owm_key_clear` | checkbox（值 `"1"`） | 勾選時清除 API Key（停用天氣提醒） |
| `owm_city` | string | 城市名稱（長度 ≤ 64） |
| `tz` | string | POSIX 時區字串（長度 ≤ 64） |
| `ntp` | NTP 伺服器 | hostname（長度 ≤ 128） |
| `cal_factor` | float | 校正係數，有效範圍 **> 100** |
| `step_on` | float | Step On Threshold，有效範圍 **1.0–200.0 kg** |
| `step_off` | float | Step Off Threshold，有效範圍 **0.1–100.0 kg** |
| `match_tol` | float | Match Tolerance，有效範圍 **0.5–30.0 kg** |
| `cooldown` | int | Cooldown（ms），有效範圍 **500–30000** |
| `user_count` | int | 使用者總數（0–10） |
| `user1_name` | string | 使用者 1 名稱 |
| `user1_weight` | float | 使用者 1 體重，有效範圍 **5.0–300.0 kg** |
| … | | （user2–user5 同格式） |

> **機密欄位（`wifi_pass`、`line_token`、`line_to`、`owm_key`）**：空值不更新現有設定；若需清除，需同時傳入對應的 `*_clear=1`。數值欄位超出有效範圍時，忽略該欄位並保留原值。

儲存完成後，裝置自動呼叫 `ESP.restart()` 重啟。

---

## 錯誤處理

| HTTP 狀態碼 | 情境 |
|------------|------|
| 200 | 成功 |
| 400 | 缺少必要參數或參數值無效 |
| 401 | 需要 HTTP Basic Auth（`adminPassword` 已設定，但未提供或不符） |
| 403 | CSRF token 不符，或密碼錯誤 |
| 429 | 請求頻率過高（`POST /api/events/clear` 密碼嘗試超限） |
| 503 | 依賴的子系統未就緒（如 EventLogger 未初始化、ScaleManager 未就緒） |

所有 API 在裝置未連接 WiFi（AP 模式）時仍可存取（透過 AP IP `192.168.4.1`）。
