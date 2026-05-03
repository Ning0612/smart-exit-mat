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
  UserProfile*   _users      = nullptr;
  int*           _userCount  = nullptr;
  AppConfig*     _cfg        = nullptr;
  unsigned long  _lastScanMs = 0;
  static const unsigned long SCAN_COOLDOWN_MS = 5000UL;

public:
  void begin(AppConfig& cfg, UserProfile* users, int& userCount, ConfigManager& cfgMgr) {
    _cfg       = &cfg;
    _users     = users;
    _userCount = &userCount;
    _cfgMgr    = &cfgMgr;

    IPAddress apIP(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(apIP, apIP, subnet);
    bool apOk = WiFi.softAP("SmartExitMat-Setup");
    delay(200);

    if (apOk) {
      Serial.printf("[Portal] AP started — SSID: SmartExitMat-Setup  IP: %s\n",
                    WiFi.softAPIP().toString().c_str());
    } else {
      Serial.println("[Portal] ERROR: softAP() failed — AP may not be visible");
    }

    _dns = new DNSServer();
    _dns->start(53, "*", apIP);

    _server = new WebServer(80);
    _attachRoutes(true);

    Serial.println("[Portal] HTTP server started");
  }

  void beginSTA(AppConfig& cfg, UserProfile* users, int& userCount, ConfigManager& cfgMgr) {
    if (_server) return;
    _cfg       = &cfg;
    _users     = users;
    _userCount = &userCount;
    _cfgMgr    = &cfgMgr;

    _server = new WebServer(80);
    _attachRoutes(false);

    Serial.printf("[Portal] HTTP server started (STA) on http://%s\n",
                  WiFi.localIP().toString().c_str());
  }

  void handleClient() {
    if (_dns)    _dns->processNextRequest();
    if (_server) _server->handleClient();
  }

private:
  void _attachRoutes(bool allowScan) {
    _server->on("/",     HTTP_GET,  [this]() { _handleRoot(); });
    if (allowScan) {
      _server->on("/scan", HTTP_GET, [this]() { _handleScan(); });
    } else {
      _server->on("/scan", HTTP_GET, [this]() { _server->send(200, "application/json", "[]"); });
    }
    _server->on("/save", HTTP_POST, [this]() { _handleSave(); });
    _server->onNotFound(           [this]() { _handleRoot(); });
    _server->begin();
  }

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

  void _handleScan() {
    if (millis() - _lastScanMs < SCAN_COOLDOWN_MS) {
      _server->send(200, "application/json", "[]");
      return;
    }
    _lastScanMs = millis();

    int n = WiFi.scanNetworks();
    if (n < 0) n = 0;

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
    String currentSsid = _escapeHtml(_cfg->wifiSsid);

    // Build user cards
    String usersHtml;
    for (int i = 0; i < *_userCount; i++) {
      int    n   = i + 1;
      String pfx = "user" + String(n);
      String chk = _users[i].atHome ? " checked" : "";
      usersHtml +=
        "<div class=\"user-card\">"
        "<div class=\"user-hdr\"><b>使用者 " + String(n) + "</b>"
        "<button type=\"button\" class=\"del-btn\" "
        "onclick=\"this.closest('.user-card').remove()\">✕</button></div>\n"
        "<label>姓名<input type=\"text\" name=\"" + pfx + "_name\" value=\""
          + _escapeHtml(_users[i].name) + "\"></label>\n"
        "<label>體重 (kg)<input type=\"number\" step=\"0.1\" name=\""
          + pfx + "_weight\" value=\"" + String(_users[i].weightKg, 1) + "\"></label>\n"
        "<label><input type=\"checkbox\" name=\"" + pfx + "_home\" value=\"1\""
          + chk + "> 目前在家</label>\n"
        "</div>\n";
    }

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
      ".user-card{border:1px solid #ddd;border-radius:4px;padding:10px;margin-bottom:8px}\n"
      ".user-hdr{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}\n"
      ".user-hdr b{font-size:.95em;color:#333}\n"
      ".del-btn{padding:4px 10px;background:#e53935;color:#fff;border:none;"
        "border-radius:3px;cursor:pointer;font-size:.8em}\n"
      ".del-btn:hover{background:#b71c1c}\n"
      ".add-btn{margin-top:8px;padding:8px 16px;background:#ff9800;color:#fff;"
        "border:none;border-radius:4px;cursor:pointer;font-size:.9em}\n"
      ".add-btn:hover{background:#e65100}\n"
      "</style></head><body>\n"
      "<h1>SmartExitMat Setup</h1>\n"
      "<form method=\"POST\" action=\"/save\">\n"

      // ── Wi-Fi ──
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
      "<label>Channel Access Token<input type=\"password\" name=\"line_token\" value=\"" + _escapeHtml(_cfg->lineChannelAccessToken) + "\"></label>\n"
      "<label>User ID (lineToId)<input type=\"password\" name=\"line_to\" value=\"" + _escapeHtml(_cfg->lineToId) + "\"></label>\n"
      "</div>\n"

      // ── 天氣提醒 ──
      "<div class=\"section\"><h2>天氣提醒（OpenWeather）</h2>\n"
      "<label>API Key<input type=\"password\" name=\"owm_key\" value=\"" + _escapeHtml(_cfg->owmApiKey) + "\"></label>\n"
      "<label>城市（英文）<input type=\"text\" name=\"owm_city\" value=\"" + _escapeHtml(_cfg->owmCity) + "\" placeholder=\"例如 Taipei、Kaohsiung\"></label>\n"
      "<small style=\"color:#888;font-size:.8em\">留空 API Key 則停用天氣提醒，城市名稱請使用英文</small>\n"
      "</div>\n"

      // ── 時間 ──
      "<div class=\"section\"><h2>時間</h2>\n"
      "<label>Timezone (POSIX)<input type=\"text\" name=\"tz\" value=\"" + _escapeHtml(_cfg->timezone) + "\"></label>\n"
      "<label>NTP Server<input type=\"text\" name=\"ntp\" value=\"" + _escapeHtml(_cfg->ntpServer) + "\"></label>\n"
      "</div>\n"

      // ── 使用者（動態） ──
      "<div class=\"section\"><h2>使用者管理</h2>\n"
      "<div id=\"users_wrap\">\n"
      + usersHtml +
      "</div>\n"
      "<button type=\"button\" class=\"add-btn\" onclick=\"addUser()\">+ 新增使用者</button>\n"
      "<input type=\"hidden\" name=\"user_count\" id=\"user_count\" value=\"" + String(*_userCount) + "\">\n"
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

      "<script>\n"
      "var _nextIdx=" + String(*_userCount + 1) + ";\n"

      // 掃描 Wi-Fi
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

      // 新增使用者
      "function addUser(){\n"
      "  var cards=document.querySelectorAll('.user-card');\n"
      "  if(cards.length>=5){alert('最多 5 位使用者');return;}\n"
      "  var idx=_nextIdx++;\n"
      "  var d=document.createElement('div');\n"
      "  d.className='user-card';\n"
      "  var tpl='<div class=\"user-hdr\"><b>使用者 #</b>'\n"
      "    +'<button type=\"button\" class=\"del-btn\">✕</button></div>'\n"
      "    +'<label>姓名<input type=\"text\" name=\"userIDX_name\" placeholder=\"輸入姓名\"></label>'\n"
      "    +'<label>體重 (kg)<input type=\"number\" step=\"0.1\" name=\"userIDX_weight\" value=\"60.0\"></label>'\n"
      "    +'<label><input type=\"checkbox\" name=\"userIDX_home\" value=\"1\"> 目前在家</label>';\n"
      "  d.innerHTML=tpl.replace(/IDX/g,idx);\n"
      "  d.querySelector('.del-btn').onclick=function(){this.closest('.user-card').remove();};\n"
      "  document.getElementById('users_wrap').appendChild(d);\n"
      "}\n"

      // 送出前重新編號，確保 user1_xxx ... userN_xxx 連續
      "document.querySelector('form').addEventListener('submit',function(){\n"
      "  document.querySelectorAll('.user-card').forEach(function(card,i){\n"
      "    var n=i+1;\n"
      "    card.querySelectorAll('input').forEach(function(el){\n"
      "      if(el.name)el.name=el.name.replace(/user\\d+_/,'user'+n+'_');\n"
      "    });\n"
      "    var b=card.querySelector('.user-hdr b');\n"
      "    if(b)b.textContent='使用者 '+n;\n"
      "  });\n"
      "  document.getElementById('user_count').value=document.querySelectorAll('.user-card').length;\n"
      "});\n"
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
    if (_server->hasArg("owm_key"))  _cfg->owmApiKey = _server->arg("owm_key");
    if (_server->hasArg("owm_city")) _cfg->owmCity   = _server->arg("owm_city");

    // 解析多使用者
    int newCount = 0;
    if (_server->hasArg("user_count")) {
      newCount = _server->arg("user_count").toInt();
      if (newCount < 0) newCount = 0;
      if (newCount > MAX_USERS) newCount = MAX_USERS;
    }
    *_userCount = newCount;

    for (int i = 0; i < newCount; i++) {
      int    n   = i + 1;
      String pfx = "user" + String(n);
      _users[i].id = String(n);
      if (_server->hasArg(pfx + "_name"))
        _users[i].name = _server->arg(pfx + "_name");
      if (_server->hasArg(pfx + "_weight"))
        _users[i].weightKg = _server->arg(pfx + "_weight").toFloat();
      _users[i].atHome = _server->hasArg(pfx + "_home");
    }
    // 清除超出範圍的使用者記憶體
    for (int i = newCount; i < MAX_USERS; i++) {
      _users[i] = UserProfile{};
    }

    _cfgMgr->saveConfig(*_cfg);
    _cfgMgr->saveUserCount(newCount);
    for (int i = 0; i < newCount; i++) {
      _cfgMgr->saveUser(_users[i]);
      _cfgMgr->saveUserState(_users[i]);
    }

    _server->send(200, "text/html; charset=utf-8",
      "<!DOCTYPE html><html><body><h2>儲存成功！重新啟動中...</h2></body></html>");
    delay(1500);
    ESP.restart();
  }
};
