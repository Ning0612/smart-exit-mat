# 疑難排解

## 診斷工具

### Serial 監控

絕大多數問題都可透過 Serial 輸出診斷。開啟方法：

```bash
pio device monitor   # 115200 baud
```

### 常見 Serial 前綴對照

| 前綴 | 模組 |
|------|------|
| `[Boot]` | 開機流程 |
| `[WiFi]` | WiFi 連線 |
| `[Scale]` | HX711 秤 |
| `[Weight]` | 重量讀值（每次取樣） |
| `[Step]` | 踩踏事件狀態機 |
| `[State]` | 使用者狀態切換 |
| `[NTP]` | 時間同步 |
| `[Weather]` | 天氣查詢 |
| `[LINE]` | LINE Bot 通知 |
| `[EventLogger]` | 事件日誌 |
| `[WeightAdapt]` | 體重自學習 |
| `[Config]` | 設定讀寫 |
| `[ConfigPortal]` | Web 設定頁 |

---

## 硬體問題

### HX711 讀值不穩定（持續跳動）

**症狀**：Serial `[Weight]` 每次數值相差超過 2–3 kg，無法穩定

**診斷**：
```
[Weight] 71.23 kg
[Weight] 58.91 kg    ← 差距過大
[Weight] 70.88 kg
```

**排查順序**：
1. **接線**：確認 DT（GPIO26）、SCK（GPIO25）、VCC（3.3V）、GND 四線均牢固連接
2. **Load Cell 安裝**：確認四顆感測器固定於穩定平面，地墊不晃動
3. **電源品質**：換用品質較好的 USB 線和充電頭；劣質 USB 線壓降大，導致 3.3V 供電不穩
4. **去耦合電容**：在 HX711 VCC 與 GND 之間焊接 100 µF 電解電容
5. **增加取樣數**：修改 `main.cpp` 第 150 行 `readWeightKg(3)` → `readWeightKg(10)`（代價：延遲增加）

---

### HX711 讀值為負數

**症狀**：`[Weight] -45.23 kg`，校正 API 回傳 `"wiring reversed? raw=-..."`

**原因**：A+/A- 訊號線接反

**解決**：將 HX711 的 A+ 和 A- 接線對調

---

### 重量讀值偏差大（校正後仍不準）

**症狀**：顯示 65 kg，實際 72 kg（固定偏差），或誤差隨重量增大

**固定偏差（如始終差 7 kg）**：
- 空秤時 tare 未成功（零點偏移）
- 解決：重啟裝置（自動執行 tare），等待 Serial 顯示 `[Scale] tare OK`

**線性誤差（越重偏差越大）**：
- Calibration Factor 不準確
- 解決：重新執行校正（`/api/calibrate?w=<體重>`）

---

## WiFi 問題

### WiFi 連線失敗

**症狀**：
```
[WiFi] Connecting to MyHome
....................................
[WiFi] Failed — entering AP mode
```

**排查**：
1. **頻段**：確認路由器已開啟 **2.4 GHz** 頻段（ESP32 不支援 5 GHz）
2. **SSID / 密碼**：重新進入設定頁確認，特別注意大小寫和特殊字元
3. **訊號強度**：ESP32 靠近路由器測試，排除訊號過弱問題
4. **MAC 過濾**：若路由器開啟 MAC 白名單，需將 ESP32 的 MAC 加入
5. **路由器設定**：關閉 Client Isolation（有時稱 AP 隔離），確保裝置可在區網中被發現

**取得 ESP32 MAC 位址**（在 Serial 確認）：
```cpp
// 臨時加入 setup() 觀察
Serial.println(WiFi.macAddress());
```

---

### 無法存取設定頁 / 儀表板

**症狀**：連上 WiFi 後，瀏覽器無法開啟 `http://192.168.x.x`

**排查**：
1. 確認電腦/手機與地墊在**同一 WiFi 下**（不同 SSID 或 VPN 下可能無法互通）
2. Serial 確認裝置已獲得 IP（`[WiFi] Connected, IP: x.x.x.x`）
3. 嘗試 `ping x.x.x.x` 確認網路可達
4. 部分路由器的「AP 隔離」（Client Isolation）功能會阻止裝置間通訊，請關閉

---

## Web UI 認證問題

### 瀏覽器一直要求輸入帳號密碼

**原因**：設定頁已設定管理員密碼，啟用了 HTTP Basic Auth。

**解決**：
- 帳號固定為 `admin`，密碼為設定頁「安全設定」中設定的管理員密碼
- 若忘記密碼：長按 GPIO0 3 秒觸發硬體重置，裝置重啟後管理密碼清空，可重新設定

---

### 沒有跳出登入視窗，只顯示網頁無法使用

**症狀**：從 LINE 訊息點開設定頁連結後，沒有出現帳號密碼視窗，頁面顯示「網頁無法使用」或 `net::ERR_HTTP_RESPONSE_CODE_FAILURE`。

**原因**：LINE App 內建瀏覽器不支援本專案使用的 HTTP Basic Auth 登入彈窗。

**解決**：
- 將設定頁網址複製到 Chrome、Safari、Edge 等外部瀏覽器開啟
- 確認網址使用 `http://`，不是 `https://`
- 登入時帳號固定為 `admin`，密碼為設定的管理員密碼

---

### 忘記管理員密碼

長按 **BOOT 鍵（GPIO0）** 超過 3 秒後**放開**（必須放開才會重啟，持續按住會進入 bootloader download 模式），裝置將清空 WiFi 憑證、AP 密碼（重置為 `12345678`）與管理員密碼（清空）並重啟進入 AP 模式。  
重新透過 AP 模式設定頁輸入新的管理員密碼即可。

---

## LINE 通知問題

### 沒有收到任何 LINE 通知

**排查順序**：

1. **Token 有效性**：登入 LINE Developers Console，確認 Channel Access Token 未過期
2. **To ID 格式**：
   - 個人：以 `U` 開頭，長度 33 字元
   - 群組：以 `C` 開頭，長度 33 字元
3. **Bot 狀態**：確認 Bot 仍在好友清單或群組中（若移除 Bot，通知無法送達）
4. **Serial 錯誤**：查看是否有 `[LINE] HTTP xxx` 或連線錯誤

**常見 HTTP 錯誤碼**：

| 錯誤碼 | 原因 |
|--------|------|
| 400 | 請求格式錯誤（通常是 Token 或 To ID 格式問題） |
| 401 | Token 無效或已過期 |
| 403 | Bot 未授權（To ID 對應的使用者已封鎖 Bot） |
| 429 | 超出 API 速率限制（免費方案有月訊息上限） |

---

### 上線通知收到，但踩踏通知沒有

**症狀**：裝置上線有通知，踩踏地墊沒反應

**排查**：
1. Serial 確認 `[Step] event weight=...` 是否出現（若無，踩踏事件未觸發）
2. 若有 Step 事件但無 LINE：查看 `[LINE]` 相關輸出
3. 檢查 Step On Threshold 設定是否過高（建議不超過體重的 1/7）

---

### 未知使用者通知過於頻繁

**症狀**：每次踩踏都收到「未知使用者」警示

**原因**：
- 使用者體重與登錄值差距超過 Match Tolerance
- Match Tolerance 設定過小

**解決**：
1. Serial 查看 `[Step] event weight=X.X kg` 的實際讀值
2. 比對設定頁中的使用者登錄體重
3. 若差距固定，更新登錄體重
4. 若差距不定，重新執行校正

---

## 事件日誌問題

### Dashboard 沒有事件記錄

**症狀**：踩踏觸發了 LINE 通知，但儀表板報表為空

**最常見原因**：NTP 未同步

```
[EventLogger] skipped — NTP not synced
```

`EventLogger::log()` 在 `time(nullptr) < 1,000,000,000` 時（2001 年以前）會靜默丟棄記錄，不寫入假時間戳。

**解決**：
1. 確認 NTP 伺服器可達（`time.google.com`）
2. Serial 確認 `[NTP] sync OK`
3. 若 NTP 持續失敗，檢查路由器防火牆是否封鎖 UDP 123 port

---

### LittleFS 掛載失敗

**症狀**：
```
[LittleFS] mount failed
```

或事件日誌完全無法使用。

**原因**：Flash 分區格式與 `partitions.csv` 不符（殘留舊分區表）

**解決**（會清除所有資料）：
```bash
pio run -t erase
pio run --target upload
```

> ⚠️ 擦除後 NVS 設定（WiFi、LINE Token、使用者、狀態）和事件日誌全部清空，需重新設定。

---

### 事件日誌正確但 Dashboard 查不到

**症狀**：Serial 確認事件已記錄，但 `/api/events` 回傳 `"count": 0`

**排查**：
1. 確認 `view`、`date`、`year`、`month` 參數格式正確
2. 確認裝置時區設定正確（台灣應為 `CST-8`），否則事件時間與查詢範圍不在同一天
3. `week` 視圖的 `date` 必須傳**該週的週一**日期

---

## 使用者識別問題

### 踩踏後識別到錯誤使用者

**症狀**：Lance 踩踏，卻切換了 Alice 的狀態

**分析**：
1. Serial 查看 `[Step] event weight=X.X kg`
2. 比對 Lance 和 Alice 的登錄體重差距
3. 若差距 < `matchToleranceKg × 2`（如差距 4 kg，容差 3 kg），識別容易混淆

**解決方案**：
- 縮小 Match Tolerance（如從 3 kg 改為 1.5 kg）
- 確保使用者體重差距 ≥ Match Tolerance × 2

---

### 識別後無法觸發任何動作（返回 null）

**症狀**：Serial 顯示 `[Step] unknown user`，但體重明顯在範圍內

**排查**：
1. 確認登錄體重是否正確（設定頁確認）
2. Serial 查看 `event weight` 與登錄體重差距
3. 若差距 > Match Tolerance，需調大容差或更新登錄體重

---

## 時間 / NTP 問題

### NTP 持續同步失敗

**症狀**：
```
[NTP] sync failed, retry in 5min
```

**排查**：
1. 確認 WiFi 已連線（`[WiFi] Connected`）
2. 確認路由器允許 UDP Port 123 出去到 Internet
3. 嘗試更換 NTP 伺服器（設定頁改為 `pool.ntp.org` 或 `tw.pool.ntp.org`）

---

### 時間顯示不正確（差 8 小時）

**症狀**：Serial 或 Dashboard 顯示的時間比實際少 8 小時

**原因**：時區設定為 `UTC` 而非 `CST-8`

**解決**：設定頁將 Timezone 更改為 `CST-8`

---

## 天氣問題

### 天氣資料從未更新（`has_data: false`）

**排查**：
1. 確認 OWM API Key 已填入（設定頁確認）
2. 確認城市名稱正確（如 `Taipei`，注意大小寫）
3. 若 Key 是剛申請的，等待最多 2 小時讓 Key 生效
4. Serial 查看 `[Weather] HTTP xxx`（401 = Key 無效，404 = 城市名稱錯誤）
5. 確認 WiFi 連線正常且有 Internet 存取

---

### 天氣建議不出現（出門通知無天氣資訊）

**原因**：`advisory()` 可能回傳空字串（天氣良好、溫度適中、無降雨預報）

這是正常行為，不代表天氣功能故障。只有符合特定條件（雨、極端溫度、降雨預報）才會附加建議。

---

## TLS / HTTPS 問題

### Serial 顯示 `-9984`（X509 憑證驗證失敗）

**症狀**：
```
[LINE] HTTP error -1
```
搭配 mbedTLS 輸出 `-9984 MBEDTLS_ERR_X509_CERT_VERIFY_FAILED`

**原因**：`RootCerts.h` 中的根 CA 與服務商實際使用的憑證鏈不符。

**診斷**（在可連網的電腦上執行）：
```bash
# 取出鏈中最後一張憑證（root CA）並顯示 SHA-1 指紋
openssl s_client -connect api.line.me:443 -servername api.line.me -showcerts 2>/dev/null \
  | awk 'BEGIN{c=""} /-----BEGIN CERTIFICATE-----/{c=""} {c=c"\n"$0} /-----END CERTIFICATE-----/{last=c} END{print last}' \
  | openssl x509 -noout -fingerprint -sha1

openssl s_client -connect api.openweathermap.org:443 -servername api.openweathermap.org -showcerts 2>/dev/null \
  | awk 'BEGIN{c=""} /-----BEGIN CERTIFICATE-----/{c=""} {c=c"\n"$0} /-----END CERTIFICATE-----/{last=c} END{print last}' \
  | openssl x509 -noout -fingerprint -sha1
```

比對輸出的 SHA-1 指紋與 `include/RootCerts.h` 中的註解值：
- `LINE_ROOT_CA`：DigiCert Global Root G2（SHA-1：`DF:3C:24:F9:...`）+ DigiCert Global Root G3
- `OWM_ROOT_CA`：USERTrust RSA Certification Authority（SHA-1：`2B:8F:1B:57:...`）

若指紋不符，服務商已更換憑證鏈，需更新 `RootCerts.h` 中的 PEM 並重新燒錄。

---

### Serial 顯示 `-15074`（ASN1 解析失敗）

**症狀**：mbedTLS 輸出 `-15074 MBEDTLS_ERR_PK_INVALID_ALG`

**原因**：`RootCerts.h` 中的 PEM 資料損毀或格式錯誤（非合法 DER 編碼的 X.509 憑證）。

**解決**：從官方或可信來源重新取得根 CA PEM，確認 Base64 內容完整，並重新燒錄。

---

### NTP 同步前 TLS 無法驗證

TLS 憑證驗證需要正確的系統時間。若 NTP 同步尚未完成，mbedTLS 可能因憑證時間驗證失敗而拒絕連線。

`main.cpp` 已確保 `g_timeMgr.begin()`（NTP）在 LINE 通知之前完成，正常情況下不會發生此問題。若自行擴充功能，請遵守相同順序。
