#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "AppTypes.h"
#include "ConfigManager.h"

class ConfigPortal {
  WebServer*     _server     = nullptr;
  DNSServer*     _dns        = nullptr;
  ConfigManager* _cfgMgr     = nullptr;
  UserProfile*   _user       = nullptr;
  AppConfig*     _cfg        = nullptr;
  unsigned long  _lastScanMs = 0;
  static const unsigned long SCAN_COOLDOWN_MS = 5000UL;

public:
  void begin(AppConfig& cfg, UserProfile& user, ConfigManager& cfgMgr) {
    _cfg    = &cfg;
    _user   = &user;
    _cfgMgr = &cfgMgr;

    IPAddress apIP(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    // 清除先前可能殘留的 STA 連線狀態，再切換到 AP_STA
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIP, apIP, subnet);
    bool apOk = WiFi.softAP("SmartExitMat-Setup");
    delay(200);  // 等待 AP 就緒

    if (apOk) {
      Serial.printf("[Portal] AP started — SSID: SmartExitMat-Setup  IP: %s\n",
                    WiFi.softAPIP().toString().c_str());
    } else {
      Serial.println("[Portal] ERROR: softAP() failed — AP may not be visible");
    }

    _dns = new DNSServer();
    _dns->start(53, "*", apIP);

    _server = new WebServer(80);
    _server->on("/",     HTTP_GET,  [this]() { _handleRoot(); });
    _server->on("/scan", HTTP_GET,  [this]() { _handleScan(); });
    _server->on("/save", HTTP_POST, [this]() { _handleSave(); });
    _server->onNotFound(            [this]() { _handleRoot(); });
    _server->begin();

    Serial.println("[Portal] HTTP server started");
  }

  void handleClient() {
    if (_dns)    _dns->processNextRequest();
    if (_server) _server->handleClient();
  }

private:
  // HTML attribute escape：防止 value="..." 被注入
  String _escapeHtml(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (unsigned int i = 0; i < s.length(); i++) {
      char c = s[i];
      if      (c == '&')  out += "&amp;";
      else if (c == '"')  out += "&quot;";
      else if (c == '<')  out += "&lt;";
      else if (c == '>')  out += "&gt;";
      else                out += c;
    }
    return out;
  }

  // JSON string escape（用於 /scan 回應中的 SSID，含控制字元 0x00-0x1F）
  String _escapeJson(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (unsigned int i = 0; i < s.length(); i++) {
      unsigned char c = (unsigned char)s[i];
      if      (c == '"')  out += "\\\"";
      else if (c == '\\') out += "\\\\";
      else if (c == '\n') out += "\\n";
      else if (c == '\r') out += "\\r";
      else if (c < 0x20) {
        char buf[7];
        snprintf(buf, sizeof(buf), "\\u%04x", c);
        out += buf;
      } else {
        out += (char)c;
      }
    }
    return out;
  }

  // 掃描 Wi-Fi 並回傳 JSON 陣列（由使用者主動觸發，阻塞約 2-3 秒）
  void _handleScan() {
    // 5 秒 cooldown 防止重複掃描持續佔用 radio
    if (millis() - _lastScanMs < SCAN_COOLDOWN_MS) {
      _server->send(200, "application/json", "[]");
      return;
    }
    _lastScanMs = millis();

    int n = WiFi.scanNetworks();
    if (n < 0) n = 0;

    // 去重複，最多 20 筆；scanNetworks 已依 RSSI 由強到弱排序
    String ssids[20];
    int count = 0;
    for (int i = 0; i < n && count < 20; i++) {
      String ssid = WiFi.SSID(i);
      if (ssid.isEmpty()) continue;
      bool dup = false;
      for (int j = 0; j < count; j++) {
        if (ssids[j] == ssid) { dup = true; break; }
      }
      if (!dup) ssids[count++] = ssid;
    }
    WiFi.scanDelete();

    String json = "[";
    for (int i = 0; i < count; i++) {
      if (i > 0) json += ",";
      json += "\"" + _escapeJson(ssids[i]) + "\"";
    }
    json += "]";

    _server->send(200, "application/json", json);
  }

  void _handleRoot() {
    String atHomeChecked = _user->atHome ? " checked" : "";
    String currentSsid   = _escapeHtml(_cfg->wifiSsid);

    String html =
      "<!DOCTYPE html>\n"
      "<html lang=\"zh-TW\"><head>\n"
      "<meta charset=\"UTF-8\">\n"
      "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
      "<title>SmartExitMat Setup</title>\n"
      "<style>\n"
      "body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 16px;background:#f5f5f5}\n"
      "h1{font-size:1.3em;color:#333}\n"
      "label{display:block;margin-top:14px;font-size:.9em;color:#555}\n"
      "input[type=text],input[type=password],input[type=number],select{"
        "width:100%;box-sizing:border-box;padding:8px;border:1px solid #ccc;"
        "border-radius:4px;background:#fff}\n"
      "input[type=submit]{margin-top:20px;width:100%;padding:10px;"
        "background:#4caf50;color:#fff;border:none;border-radius:4px;font-size:1em;cursor:pointer}\n"
      "input[type=submit]:hover{background:#388e3c}\n"
      ".section{margin-top:20px;padding:12px;background:#fff;border-radius:6px;"
        "box-shadow:0 1px 3px rgba(0,0,0,.1)}\n"
      "h2{font-size:1em;margin:0 0 8px;color:#444}\n"
      ".scan-row{display:flex;gap:6px;margin-top:6px;align-items:stretch}\n"
      ".scan-row select{flex:1}\n"
      ".scan-btn{padding:8px 12px;background:#2196f3;color:#fff;border:none;"
        "border-radius:4px;cursor:pointer;white-space:nowrap;font-size:.9em}\n"
      ".scan-btn:disabled{background:#90caf9;cursor:default}\n"
      "</style></head><body>\n"
      "<h1>SmartExitMat Setup</h1>\n"
      "<form method=\"POST\" action=\"/save\">\n"

      // ── Wi-Fi（含掃描）──
      "<div class=\"section\"><h2>Wi-Fi</h2>\n"
      "<label>SSID\n"
      "<div class=\"scan-row\">\n"
      "  <select id=\"ssid_sel\" onchange=\"if(this.value)document.getElementById('wifi_ssid').value=this.value\">\n"
      "    <option value=\"\">-- 點擊掃描後選擇 --</option>\n"
      "  </select>\n"
      "  <button type=\"button\" class=\"scan-btn\" id=\"scan_btn\" onclick=\"doScan()\">掃描 Wi-Fi</button>\n"
      "</div>\n"
      "<input type=\"text\" name=\"wifi_ssid\" id=\"wifi_ssid\" value=\"" + currentSsid + "\" placeholder=\"或直接輸入 SSID\">\n"
      "</label>\n"
      "<label>Password<input type=\"password\" name=\"wifi_pass\" value=\"" + _escapeHtml(_cfg->wifiPassword) + "\"></label>\n"
      "</div>\n"

      // ── LINE Bot ──
      "<div class=\"section\"><h2>LINE Bot</h2>\n"
      "<label>Channel Access Token<input type=\"text\" name=\"line_token\" value=\"" + _escapeHtml(_cfg->lineChannelAccessToken) + "\"></label>\n"
      "<label>User ID (lineToId)<input type=\"text\" name=\"line_to\" value=\"" + _escapeHtml(_cfg->lineToId) + "\"></label>\n"
      "</div>\n"

      // ── 時間 ──
      "<div class=\"section\"><h2>時間</h2>\n"
      "<label>Timezone (POSIX)<input type=\"text\" name=\"tz\" value=\"" + _escapeHtml(_cfg->timezone) + "\"></label>\n"
      "<label>NTP Server<input type=\"text\" name=\"ntp\" value=\"" + _escapeHtml(_cfg->ntpServer) + "\"></label>\n"
      "</div>\n"

      // ── 使用者 ──
      "<div class=\"section\"><h2>使用者 1</h2>\n"
      "<label>姓名<input type=\"text\" name=\"user1_name\" value=\"" + _escapeHtml(_user->name) + "\"></label>\n"
      "<label>體重 (kg)<input type=\"number\" step=\"0.1\" name=\"user1_weight\" value=\"" + String(_user->weightKg, 1) + "\"></label>\n"
      "<label><input type=\"checkbox\" name=\"user1_home\" value=\"1\"" + atHomeChecked + "> 目前在家</label>\n"
      "</div>\n"

      // ── 秤設定 ──
      "<div class=\"section\"><h2>秤與偵測設定</h2>\n"
      "<label>Calibration Factor<input type=\"number\" step=\"0.01\" name=\"cal_factor\" value=\"" + String(_cfg->calibrationFactor, 2) + "\"></label>\n"
      "<label>踩踏門檻 (kg)<input type=\"number\" step=\"0.1\" name=\"step_on\" value=\"" + String(_cfg->stepOnThresholdKg, 1) + "\"></label>\n"
      "<label>離開門檻 (kg)<input type=\"number\" step=\"0.1\" name=\"step_off\" value=\"" + String(_cfg->stepOffThresholdKg, 1) + "\"></label>\n"
      "<label>比對容差 (kg)<input type=\"number\" step=\"0.1\" name=\"match_tol\" value=\"" + String(_cfg->matchToleranceKg, 1) + "\"></label>\n"
      "<label>Cooldown (ms)<input type=\"number\" name=\"cooldown\" value=\"" + String(_cfg->cooldownMs) + "\"></label>\n"
      "</div>\n"

      "<input type=\"submit\" value=\"儲存並重啟\">\n"
      "</form>\n"

      // ── 掃描 JS ──
      "<script>\n"
      "function doScan(){\n"
      "  var btn=document.getElementById('scan_btn');\n"
      "  var sel=document.getElementById('ssid_sel');\n"
      "  btn.disabled=true;btn.textContent='掃描中...';\n"
      "  sel.innerHTML='<option value=\"\">掃描中，請稍候...</option>';\n"
      "  fetch('/scan').then(function(r){return r.json();}).then(function(nets){\n"
      "    var cur=document.getElementById('wifi_ssid').value;\n"
      "    sel.innerHTML='<option value=\"\">-- 選擇網路 --</option>';\n"
      "    nets.forEach(function(s){\n"
      "      var o=document.createElement('option');\n"
      "      o.value=s;o.textContent=s;\n"
      "      if(s===cur)o.selected=true;\n"
      "      sel.appendChild(o);\n"
      "    });\n"
      "    btn.disabled=false;btn.textContent='重新掃描';\n"
      "  }).catch(function(){\n"
      "    sel.innerHTML='<option value=\"\">掃描失敗，請重試</option>';\n"
      "    btn.disabled=false;btn.textContent='掃描 Wi-Fi';\n"
      "  });\n"
      "}\n"
      "</script>\n"
      "</body></html>";

    _server->send(200, "text/html; charset=utf-8", html);
  }

  void _handleSave() {
    if (_server->hasArg("wifi_ssid"))    _cfg->wifiSsid               = _server->arg("wifi_ssid");
    if (_server->hasArg("wifi_pass"))    _cfg->wifiPassword            = _server->arg("wifi_pass");
    if (_server->hasArg("line_token"))   _cfg->lineChannelAccessToken  = _server->arg("line_token");
    if (_server->hasArg("line_to"))      _cfg->lineToId                = _server->arg("line_to");
    if (_server->hasArg("tz"))           _cfg->timezone                = _server->arg("tz");
    if (_server->hasArg("ntp"))          _cfg->ntpServer               = _server->arg("ntp");
    if (_server->hasArg("cal_factor"))   _cfg->calibrationFactor       = _server->arg("cal_factor").toFloat();
    if (_server->hasArg("step_on"))      _cfg->stepOnThresholdKg       = _server->arg("step_on").toFloat();
    if (_server->hasArg("step_off"))     _cfg->stepOffThresholdKg      = _server->arg("step_off").toFloat();
    if (_server->hasArg("match_tol"))    _cfg->matchToleranceKg        = _server->arg("match_tol").toFloat();
    if (_server->hasArg("cooldown")) {
      int cd = _server->arg("cooldown").toInt();
      if (cd > 0) _cfg->cooldownMs = (unsigned long)cd;
    }

    if (_server->hasArg("user1_name"))   _user->name     = _server->arg("user1_name");
    if (_server->hasArg("user1_weight")) _user->weightKg = _server->arg("user1_weight").toFloat();
    _user->atHome = _server->hasArg("user1_home");

    _cfgMgr->saveConfig(*_cfg);
    _cfgMgr->saveUser(*_user);
    _cfgMgr->saveUserState(*_user);

    _server->send(200, "text/html; charset=utf-8",
      "<!DOCTYPE html><html><body><h2>儲存成功！重新啟動中...</h2></body></html>");
    delay(1500);
    ESP.restart();
  }
};
