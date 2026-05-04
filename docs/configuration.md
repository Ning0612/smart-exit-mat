# 設定說明

## 進入設定頁

### 首次設定（AP 模式）

首次燒錄或清空 WiFi 憑證後，裝置因無 SSID 可連，自動啟動 AP 熱點：

1. 手機或電腦搜尋 WiFi → 選擇 **`SmartExitMat-Setup`**，輸入 WPA2 密碼（**預設 `12345678`**；可在設定頁「安全設定」區塊修改，開機時顯示於 Serial：`[Portal] AP started — ... Password: XXXXXXXX`）
2. 使用外部瀏覽器（Chrome、Safari、Edge 等）開啟：`http://192.168.4.1`
   > LINE App 內建瀏覽器不支援 HTTP Basic Auth 登入彈窗；若已設定管理員密碼，請改用外部瀏覽器開啟。

### 後續設定（STA 模式）

WiFi 連線成功後，在**同一區域網路**下：
1. 從上線 LINE 通知取得裝置 IP（格式：`http://192.168.x.x`）
2. 或從路由器後台的「已連線裝置」查詢
3. 使用外部瀏覽器直接開啟該 IP（不要使用 LINE App 內建瀏覽器）

---

## WiFi 設定

| 欄位 | 說明 |
|------|------|
| WiFi SSID | 家用 WiFi 名稱（區分大小寫） |
| WiFi Password | WiFi 密碼（留空表示開放網路） |

### 注意事項

- ESP32 **只支援 2.4 GHz** 頻段，5 GHz 網路無法連接
- 若路由器開啟了「隱藏 SSID」，需手動輸入名稱（掃描不會顯示）
- SSID 長度上限 32 字元，密碼上限 64 字元（WPA2 限制）
- 設定頁的「掃描 Wi-Fi」功能**僅在 AP 模式下有效**，STA 模式下不執行實際掃描
- **密碼欄位行為（安全防護）**：設定頁不回顯現有密碼，欄位顯示「已設定（留空不變更）」；直接送出空白欄位**不會清除**現有密碼；若需連接開放網路（無密碼），請勾選欄位下方「清除密碼（連接開放網路）」checkbox

---

## LINE Bot 設定

### 取得 Channel Access Token

1. 前往 [LINE Developers Console](https://developers.line.biz/)
2. 登入並建立一個 Provider（首次使用）
3. 建立新 Channel，類型選 **「Messaging API」**
4. 進入 Channel 設定 → 「Messaging API」分頁
5. 在「Channel access token」點選「Issue」產生 Token（長效版）
6. 複製 Token（格式為長串英數字）

### 取得 To ID（接收通知的對象 ID）

**取得個人 User ID：**
1. 在 LINE Developers Console 的「Basic settings」分頁
2. 找到「Your user ID」（格式：`Uxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx`，以 `U` 開頭）

**取得群組 Group ID（發送至群組）：**
1. 在群組內新增你的 Bot 帳號為成員
2. 於群組中傳送任意訊息
3. 透過 Webhook 接收的事件 JSON 中，`source.groupId` 即為 Group ID（以 `C` 開頭）

> 若只用於個人通知，直接使用個人 User ID 即可。

### LINE 通知類型

| 情境 | 訊息內容 |
|------|---------|
| 裝置上線 | 「智慧地墊已上線\n區網設定頁：http://x.x.x.x」 |
| 已知使用者出門 | 姓名、「出門了」、時間、重量、天氣建議 |
| 已知使用者回家 | 姓名、「回家了」、時間、重量 |
| 未知使用者（> 20 kg） | 「未知使用者：偵測到未登錄的踩踏，請確認」 |

未知使用者警示每 3 分鐘最多發送一次（冷卻限速）。

> **Token 欄位行為（安全防護）**：Channel Access Token 與 User ID 欄位不回顯現有值；留空送出**不會清除**現有 Token；若需停用 LINE 通知，請勾選欄位旁「清除 Token（停用 LINE 通知）」checkbox。

---

## 天氣設定（可選）

### 取得 OpenWeatherMap API Key

1. 前往 [openweathermap.org](https://openweathermap.org/api) 免費註冊帳號
2. 登入後進入 [API Keys 頁面](https://home.openweathermap.org/api_keys)
3. 預設會有一個名稱為「Default」的 Key，複製使用
4. 免費方案限制：每分鐘 60 次呼叫（本專案約每 30 分鐘查詢一次，用量極低）

> **注意**：新建立的 API Key 可能需要 10 分鐘到 2 小時才能生效，若立即使用收到 401 錯誤屬正常現象。

### 城市名稱格式

| 城市 | 填入值 |
|------|--------|
| 台北 | `Taipei` |
| 新北 | `New Taipei City` |
| 台中 | `Taichung` |
| 高雄 | `Kaohsiung` |
| 其他 | 城市英文名（可在 OWM 網站搜尋確認） |

> **城市名稱編碼**：僅允許英文字母、數字、`-`、`.`、`_`；包含空白或其他特殊字元會自動 URL 百分比編碼（如 `New Taipei City` → `New%20Taipei%20City`）。

留空 API Key 欄位可**完全停用天氣功能**，出門通知不附天氣建議。

> **API Key 欄位行為（安全防護）**：欄位不回顯現有值；留空送出**不會清除**現有 Key；若需停用天氣，請勾選「清除 API Key（停用天氣提醒）」checkbox。

---

## 時間設定

### 時區（POSIX 格式）

ESP32 的 `configTzTime()` 使用 POSIX 時區字串格式（TZ 環境變數格式）。

| 地區 | POSIX 時區字串 |
|------|--------------|
| 台灣（CST UTC+8） | `CST-8` |
| 日本（JST UTC+9） | `JST-9` |
| 英國（GMT/BST） | `GMT0BST,M3.5.0/1,M10.5.0` |
| 美東（EST/EDT） | `EST5EDT,M3.2.0,M11.1.0` |
| 美西（PST/PDT） | `PST8PDT,M3.2.0,M11.1.0` |

格式說明：`<標準時區名><與 UTC 的差（西為正）><夏令時規則>`
- `CST-8`：標準時區 CST，比 UTC 早 8 小時（無夏令時）

### NTP 伺服器

| 伺服器 | 說明 |
|--------|------|
| `time.google.com`（預設） | Google NTP，穩定度高 |
| `pool.ntp.org` | 全球 NTP Pool |
| `tw.pool.ntp.org` | 台灣節點 |

NTP 同步策略：
- 開機時阻塞等待最多 3 秒完成初次同步
- 同步失敗後每 5 分鐘重試一次
- 成功後每 24 小時定期重新同步

---

## 秤參數設定

### Calibration Factor（校正係數）

| 項目 | 說明 |
|------|------|
| 預設值 | `2280.0`（極不準確，**首次使用前必須重新校正**） |
| 意義 | HX711 原始 ADC 讀值 ÷ 實際重量（kg）的比值 |
| 計算 | 站上地墊後呼叫 `/api/calibrate?w=<你的體重>` 自動計算 |
| 有效範圍 | **> 100**（否則設定不會被接受） |

詳細校正方法請參閱 [calibration.md](calibration.md)。

### Step On Threshold（踩踏觸發門檻）

| 項目 | 說明 |
|------|------|
| 預設值 | `10.0 kg` |
| 有效範圍 | **1.0–200.0 kg** |
| 作用 | 地墊讀值超過此值時，進入 MEASURING 狀態 |
| 調整建議 | 若有寵物（貓約 3–6 kg）常觸發誤報，可調高至 15–20 kg |

### Step Off Threshold（離開觸發門檻）

| 項目 | 說明 |
|------|------|
| 預設值 | `3.0 kg` |
| 有效範圍 | **0.1–100.0 kg** |
| 作用 | MEASURING 狀態下讀值低於此值時，觸發踩踏事件 |
| 調整建議 | 需小於 Step On Threshold；若踩踏靈敏度不足，可嘗試調高 |

### Match Tolerance（識別容差）

| 項目 | 說明 |
|------|------|
| 預設值 | `3.0 kg` |
| 有效範圍 | **0.5–30.0 kg** |
| 作用 | identify() 中允許偵測重量與登錄體重的最大差距 |
| 調整建議 | 若家庭成員體重差距較小（< 5 kg），應縮小容差（如 1.5–2.0 kg）；若識別率低，可放寬至 5.0 kg |

**重要**：若兩位使用者體重差距小於 `Match Tolerance × 2`，可能互相混淆。
例如：A=65 kg、B=67 kg、Tolerance=3.0 → 兩者皆在對方容差範圍內，識別準確度下降。

### Cooldown（冷卻時間）

| 項目 | 說明 |
|------|------|
| 預設值 | `1500 ms` |
| 有效範圍 | **500–30000 ms** |
| 作用 | COOLDOWN 狀態持續時間，期間忽略所有重量變化 |
| 調整建議 | 若踩踏時偶爾觸發兩次事件，可增大（如 3000 ms） |

---

## 使用者管理

### 新增使用者

1. 設定頁下方「使用者管理」區塊
2. 點選「新增使用者」按鈕
3. 填入**姓名**與**體重（kg）**（有效範圍：**5–300 kg**，超出範圍的值不會被接受）
4. 最多可新增 5 位（設定頁 UI 限制）
5. 點選「儲存設定」完成

> NVS 實際可儲存最多 10 位使用者（`MAX_USERS = 10`），但設定頁介面目前限制 5 位。

### 刪除使用者

目前設定頁支援每個使用者卡片的「刪除」按鈕，刪除後需儲存設定。

### 體重登錄建議

- 使用**穿著平常衣物、不穿外套**時的體重（與出門狀態一致）
- 早晨體重與晚間體重約相差 0.5–1.5 kg，建議取一天中最常踩踏時段的體重
- 若多位使用者體重相近，需搭配調整 Match Tolerance

---

## 儲存與重啟

點選設定頁底部「**儲存設定**」按鈕後：
1. 表單資料透過 `POST /save` 傳至裝置
2. 裝置將所有設定寫入 NVS（`cfg` 和 `users` 命名空間）
3. 裝置自動重啟，並嘗試連接指定 WiFi
4. 連線成功後發送上線 LINE 通知

> **機密欄位說明**：WiFi 密碼、LINE Token、LINE User ID、OWM API Key 等欄位採「留空不變更」設計，不回顯現有值。若需清除，請勾選欄位旁的「清除」checkbox 再儲存。數值欄位（校正係數、門檻、Cooldown、使用者體重）若超出有效範圍，系統會忽略該欄位並保留原值。

---

## 安全設定

### AP 熱點密碼

| 項目 | 說明 |
|------|------|
| 預設值 | `12345678` |
| 長度限制 | **8–63 字元**（WPA2 PSK 標準） |
| 修改方式 | 設定頁「安全設定」區塊 → AP 密碼欄位 → 儲存重啟 |
| 顯示位置 | 開機時印於 Serial（`[Portal] AP started — ... Password: XXXXXXXX`） |

**注意**：預設密碼 `12345678` 為公開資訊，建議首次設定後立即更改。  
AP 密碼欄位採「留空不變更」設計，若需重置可長按 GPIO0 3 秒（見下方）。

### 管理員密碼（Web UI 認證）

| 項目 | 說明 |
|------|------|
| 預設值 | 空值（停用認證，所有路由開放） |
| 長度限制 | **8 字元以上**（若設定） |
| 帳號 | `admin`（固定） |
| 修改方式 | 設定頁「安全設定」區塊 → 管理員密碼欄位 → 儲存重啟 |
| 停用方式 | 勾選「清除管理密碼（停用 Web 認證）」→ 儲存 |

設定管理員密碼後，所有 Web 路由（設定頁、儀表板、API）均需透過 HTTP Basic Auth 驗證。  
瀏覽器會自動彈出登入對話框，輸入帳號 `admin` 與設定的密碼。
請使用 Chrome、Safari、Edge 等外部瀏覽器開啟設定頁；LINE App 內建瀏覽器不支援此登入彈窗，可能只會顯示「網頁無法使用」或 `net::ERR_HTTP_RESPONSE_CODE_FAILURE`。

**忘記管理員密碼**：長按 GPIO0 3 秒觸發硬體重置，裝置重啟後管理密碼清空（停用認證）。

### 硬體重置（GPIO0 長按）

長按 **BOOT 鍵（GPIO0）** 超過 3 秒後**放開**（必須放開才會重啟；持續按住會使 ESP32 進入 bootloader download 模式），裝置將清空以下設定並重啟進入 AP 模式：

| 清空項目 | 重置值 |
|---------|--------|
| WiFi SSID | 空（強制進入 AP 模式） |
| WiFi 密碼 | 空 |
| AP 熱點密碼 | `12345678`（恢復預設） |
| 管理員密碼 | 空（停用 Web 認證） |

> 其他設定（LINE Token、使用者資料、事件日誌）**不受影響**。

### TLS 根 CA 憑證

裝置與 `api.line.me`（LINE）及 `api.openweathermap.org`（OWM）的 HTTPS 通訊，均使用 `setCACert()` 進行伺服器憑證驗證，根 CA PEM 儲存於 `include/RootCerts.h`。

**驗證根 CA 是否仍符合實際伺服器**（在可連網的電腦上執行）：

```bash
# 顯示完整憑證鏈（depth 0 = leaf, depth 1 = intermediate, depth 2 = root CA）
openssl s_client -connect api.line.me:443 -servername api.line.me 2>/dev/null | head -30

# 取出鏈中最後一張憑證（root CA）並顯示 SHA-1 指紋
openssl s_client -connect api.line.me:443 -servername api.line.me -showcerts 2>/dev/null \
  | awk 'BEGIN{c=""} /-----BEGIN CERTIFICATE-----/{c=""} {c=c"\n"$0} /-----END CERTIFICATE-----/{last=c} END{print last}' \
  | openssl x509 -noout -fingerprint -sha1

# OWM 同樣方式
openssl s_client -connect api.openweathermap.org:443 -servername api.openweathermap.org -showcerts 2>/dev/null \
  | awk 'BEGIN{c=""} /-----BEGIN CERTIFICATE-----/{c=""} {c=c"\n"$0} /-----END CERTIFICATE-----/{last=c} END{print last}' \
  | openssl x509 -noout -fingerprint -sha1
```

> **注意**：`openssl s_client ... | openssl x509 -fingerprint` 只顯示第一張憑證（leaf cert）指紋，不是 root CA。上方 `awk` 指令會自動取得鏈中最後一張憑證（root CA）。

若 SHA-1 指紋不符合 `RootCerts.h` 中的註解值，表示服務商已更換憑證鏈，需更新 `RootCerts.h` 並重新燒錄。

**Serial 錯誤代碼對照**：

| 錯誤碼 | 含義 | 可能原因 |
|--------|------|---------|
| `-9984` | X509 憑證驗證失敗 | 根 CA 不符或已過期 |
| `-15074` | ASN1 解析錯誤 | PEM 格式損毀或非 DER 編碼 |

詳細說明請參閱 [docs/security.md](security.md)。
