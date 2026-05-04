# 資安防護說明

本文件記錄所有已實作的防護措施。

---

## 已實作項目

| 項目 | 說明 |
|------|------|
| AP WPA2 密碼 | 可手動設定（預設 `12345678`，最少 8 碼、最多 63 碼），印於 Serial |
| 機密不回傳瀏覽器 | WiFi 密碼、LINE Token、OWM Key 不出現在 HTML 表單值 |
| 機密清除 checkbox | 留空欄位 = 保留現有；清除需明確勾選 |
| 輸入範圍驗證 | 所有數值欄位加 `isfinite` + 邊界檢查，拒絕無效值 |
| URL 完整編碼 | `owmCity` 防止注入 OWM API query 參數 |
| JSON 控制字元 | `< 0x20` 輸出 `\u%04x`，防格式異常 |
| TLS 憑證驗證 | `LineNotifier` 與 `WeatherManager` 使用 `setCACert()`，根 CA 定義於 `RootCerts.h` |
| Web UI 認證 | HTTP Basic Auth（帳號 `admin`），由 `AppConfig.adminPassword` 控制；空值停用認證（首次設定用） |
| CSRF 防護 | `POST /save` 與 `POST /api/events/clear` 均驗證 per-boot 16 碼 nonce |
| 清除事件日誌雙重驗證 | `POST /api/events/clear` 需 HTTP Basic Auth + CSRF token + 管理員密碼明確輸入 |
| 暴力破解防護 | `POST /api/events/clear` 密碼嘗試：5 次失敗後觸發 30s 冷卻（429 Too Many Requests） |

---

## 各項目詳細說明

### AP 密碼管理

AP 密碼儲存於 NVS key `ap_pass`，預設值 `"12345678"`。長度限制 8–63 字元（WPA2 PSK 標準）。

**修改方式**：在設定頁「安全設定」區塊輸入新密碼，儲存後重啟生效。

**注意**：預設值 `12345678` 為公開資訊，建議首次設定後立即更改。若遺忘，可透過 Serial Monitor 查看（開機時印出）。

---

### TLS 憑證驗證

**LineNotifier** 與 **WeatherManager** 均以 `setCACert()` 取代原本的 `setInsecure()`。

根 CA PEM 儲存於 `include/RootCerts.h`（Flash PROGMEM，不佔 RAM）：

| 常數 | 用途 | 根 CA |
|------|------|-------|
| `LINE_ROOT_CA` | `api.line.me` | DigiCert Global Root G2（RSA）+ DigiCert Global Root G3（ECC）雙根 CA bundle |
| `OWM_ROOT_CA` | `api.openweathermap.org` | USERTrust RSA Certification Authority（Sectigo） |

**憑證驗證指令**（在可連網的電腦上執行）：

```bash
# 取出鏈中最後一張憑證（root CA）並顯示 SHA-1 指紋
# （直接 pipe 給 openssl x509 只會顯示 leaf cert，需用 awk 取最後一張）
openssl s_client -connect api.line.me:443 -servername api.line.me -showcerts 2>/dev/null \
  | awk 'BEGIN{c=""} /-----BEGIN CERTIFICATE-----/{c=""} {c=c"\n"$0} /-----END CERTIFICATE-----/{last=c} END{print last}' \
  | openssl x509 -noout -fingerprint -sha1

openssl s_client -connect api.openweathermap.org:443 -servername api.openweathermap.org -showcerts 2>/dev/null \
  | awk 'BEGIN{c=""} /-----BEGIN CERTIFICATE-----/{c=""} {c=c"\n"$0} /-----END CERTIFICATE-----/{last=c} END{print last}' \
  | openssl x509 -noout -fingerprint -sha1
```

**憑證更新**：根 CA 有效期通常 10–20 年，但若服務商更換憑證鏈，需更新 `RootCerts.h` 並重新燒錄。

**重要**：因 TLS 驗證需要正確系統時間，NTP 同步（`g_timeMgr.begin()`）**必須**在 LINE 通知之前完成。`main.cpp` 已按此順序實作。

---

### Web UI 認證

使用 ESP32 Arduino `WebServer` 內建的 HTTP Basic Authentication。

**設定方式**：
1. 進入設定頁「安全設定」區塊
2. 輸入管理員密碼（最少 8 碼）
3. 儲存後，所有路由均需以帳號 `admin` + 設定的密碼登入

**停用認證**：勾選「清除管理密碼（停用 Web 認證）」後儲存。

**注意事項**：
- `adminPassword` 空值 = 停用認證（首次設定模式，所有路由開放）
- HTTP Basic Auth 在 HTTP（非 HTTPS）下以 Base64 傳輸，同網段被動嗅探可見；在家用 WiFi 環境下仍比無認證安全
- 忘記管理密碼：長按 GPIO0 清空 WiFi 憑證，下次開機進 AP 模式時 `adminPassword` 為空（認證停用），重新設定即可

**受保護路由**：

| 路由 | 認證要求 |
|------|---------|
| `GET /` | 需認證（若密碼已設） |
| `GET /dashboard` | 需認證 |
| `GET /api/status` | 需認證 |
| `GET /api/events` | 需認證 |
| `GET /api/events/status` | 需認證 |
| `GET /api/csrf` | 需認證（取得 CSRF token） |
| `POST /api/events/clear` | 需認證 + CSRF + 管理員密碼（三重驗證） |
| `GET /api/calibrate` | 需認證 |
| `POST /save` | 需認證 + CSRF |
| `GET /scan` | 開放（AP 模式掃描需用） |

---

### CSRF 防護

`POST /save` 與 `POST /api/events/clear` 均採用 nonce 機制防止跨站請求偽造。

**實作**：
- `ConfigPortal::begin()` / `beginSTA()` 時以 `esp_random()` 產生 16 碼十六進位 token
- `POST /save`：Token 嵌入設定頁 HTML form 作為隱藏欄位 `<input type="hidden" name="_csrf">`
- `POST /api/events/clear`：客戶端先呼叫 `GET /api/csrf` 取得 token，再隨 POST body 送出
- 兩者均呼叫 `_checkCsrf()`，不符則回傳 403
- Token 為 per-boot 固定值；`/save` 必然觸發重啟，故每次儲存操作都使用新 token

**`GET /api/csrf` 的存取控制**：此端點同樣需要 HTTP Basic Auth，確保只有已認證的使用者才能取得 token，維護 CSRF 防護的完整性。

**與 Basic Auth 的關係**：若已設定 `adminPassword`，HTTP Basic Auth 本身已提供隱含 CSRF 保護（跨域請求不自動攜帶 Authorization header）；CSRF nonce 為額外防禦層。

---

### 事件日誌清除防護

`POST /api/events/clear` 為破壞性操作，採三重驗證：

1. **HTTP Basic Auth**：確認使用者身分（與其他受保護路由一致）
2. **CSRF token**：防止跨站請求偽造
3. **管理員密碼明確輸入**：需在 Dashboard Modal 中再次輸入密碼確認（`adminPassword` 為空時跳過）

**暴力破解防護**：

- 密碼嘗試失敗累計達 **5 次**後，觸發 30 秒冷卻期
- 冷卻期間所有嘗試均回傳 `HTTP 429`，不消耗配額
- 失敗計數於裝置重啟後重置
- 密碼欄位長度限制 128 字元，防止超大輸入造成記憶體壓力

**已知限制**：
- 裝置使用 HTTP（非 HTTPS），密碼與 token 以明文在網路上傳輸；在家用 WPA2 WiFi 環境下風險可接受，不建議在公共/共享網路中使用
- 冷卻計數儲存於 RAM，重啟後歸零（但 CSRF token 也會更換）

---

## 密碼長度規則

| 欄位 | 最小值 | 最大值 | 規則 |
|------|--------|--------|------|
| AP 密碼 | 8 碼 | 63 碼 | WPA2 PSK 標準；HTML minlength + 伺服器端雙重驗證 |
| 管理員密碼 | 8 碼（若設定） | — | 空值 = 停用認證；HTML minlength + 伺服器端雙重驗證 |

---

## 安全建議

```
建議初次部署後立即執行：
1. 修改 AP 密碼（設定頁 → 安全設定 → AP 密碼）
2. 設定管理員密碼（設定頁 → 安全設定 → 管理員密碼）
3. 確認 RootCerts.h 的憑證指紋符合實際伺服器
```
