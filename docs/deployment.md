# 燒錄與部署

## Flash 分區配置

本專案使用自訂分區表（`partitions.csv`），為 4 MB Flash 量身配置，**不支援 OTA（空中更新）**。

```
# Name,   Type, SubType, Offset,   Size
nvs,      data, nvs,     0x9000,  0x5000    # 20 KB  - Preferences / NVS
app0,     app,  factory, 0x10000, 0x300000  # 3 MB   - 韌體（唯一分區）
spiffs,   data, spiffs,  0x310000,0xF0000   # 960 KB - LittleFS 事件日誌
```

### 為何不用預設分區表

ESP32 預設分區表通常分配較小的 app 分區（約 1 MB）給每個 OTA 槽，並以兩個槽支援空中更新。本專案韌體體積約 1.1 MB，預設分區放不下；捨棄 OTA 後可分配 3 MB 的連續 factory 空間。

### 各分區用途

| 分區 | 大小 | 內容 |
|------|------|------|
| `nvs` | 20 KB | WiFi、LINE Token、OWM Key、時區、使用者資料、在家狀態 |
| `app0` | 3 MB | 已編譯韌體（.bin） |
| `spiffs` | 960 KB | LittleFS，儲存 `/events/YYYY-MM.jsonl` 事件日誌 |

> `spiffs` 是分區標籤名（ESP-IDF 兼容），實際格式化為 **LittleFS**（`board_build.filesystem = littlefs`）。

---

## 首次燒錄（強烈建議執行完整流程）

### 為何需要先擦除

本專案使用自訂分區佈局。若板子上已有其他韌體（不論是否使用此專案），Flash 中的分區表可能與 `partitions.csv` 不符。ESP-IDF bootloader 在讀取應用程式時依賴 Flash 0x8000 位置的分區表，若舊表與新表不吻合，LittleFS 可能掛載在錯誤偏移位置，導致：

- `[LittleFS] mount failed` 啟動錯誤
- NVS 讀取到殘留舊資料
- 事件日誌寫入位置錯誤

**結論：無論是否為全新板子，首次使用本專案前都應執行一次全片擦除。**

### 完整首次燒錄流程

```bash
# 步驟 1：全片擦除（約 10 秒）
pio run -t erase

# 步驟 2：編譯並上傳韌體
pio run --target upload

# 步驟 3：開啟 Serial 監控確認啟動正常
pio device monitor
```

預期 Serial 輸出（首次，無 WiFi 設定）：

```
[Boot] SmartExitMat starting
[Boot] No WiFi SSID — entering AP mode
[Scale] tare OK (0.00 kg)
[ConfigPortal] AP started: 192.168.4.1
```

> ⚠️ **警告**：擦除後 NVS 所有資料（WiFi、LINE Token、使用者、在家狀態）全部清空，需重新進入設定頁輸入。

---

## 標準燒錄流程（韌體更新）

更新既有裝置的韌體時，**不需要**重新擦除（除非分區表有改動）：

```bash
# 直接上傳（NVS 設定保留）
pio run --target upload
```

若修改了 `partitions.csv`，則必須重新執行完整首次燒錄流程。

---

## Serial 監控

### 連線參數

| 參數 | 值 |
|------|----|
| Baud Rate | 115200 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |
| Flow Control | None |

```bash
# PlatformIO CLI
pio device monitor

# 或指定 port（Windows 範例）
pio device monitor --port COM3 --baud 115200
```

### 常見 Serial 輸出說明

| 輸出 | 說明 |
|------|------|
| `[Boot] SmartExitMat starting` | 裝置正常啟動 |
| `[WiFi] Connecting to <SSID>` | 正在連接 WiFi |
| `[WiFi] Connected, IP: x.x.x.x` | WiFi 連線成功 |
| `[WiFi] Failed — entering AP mode` | WiFi 失敗，進入 AP 設定模式 |
| `[NTP] sync OK` | NTP 校時成功 |
| `[NTP] sync failed, retry in 5min` | NTP 失敗，5 分鐘後重試 |
| `[Weight] x.xx kg` | 目前地墊讀值（每次取樣都輸出） |
| `[Step] event weight=x.x kg` | 踩踏事件觸發 |
| `[State] Lance 出門了 @ 2025-05-04 14:30 (72.1 kg)` | 已知使用者踩踏結果 |
| `[Step] unknown user` | 未知使用者踩踏 |
| `[Weather] advisory: ...` | 天氣建議更新 |
| `[WeightAdapt] Lance 70.0->72.1 kg` | 體重自學習更新 |
| `[EventLogger] skipped — NTP not synced` | NTP 未同步，事件未記錄 |

---

## 常用指令速查

```bash
pio run                        # 僅編譯（不上傳）
pio run --target upload        # 編譯 + 上傳
pio device monitor             # 開啟 Serial 監控
pio run --target clean         # 清除 .pio/ 編譯快取
pio run -t erase               # 全片 Flash 擦除
pio device list                # 列出偵測到的序列裝置
```

> **Windows PowerShell 5.1**：`&&` 不支援，需分兩行執行或使用 Git Bash / PowerShell 7+。
