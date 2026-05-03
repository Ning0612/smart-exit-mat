#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "AppTypes.h"
#include "ConfigManager.h"
#include "WeatherManager.h"
#include "EventLogger.h"
#include "TimeManager.h"
#include "ScaleManager.h"

class ConfigPortal {
  WebServer*     _server     = nullptr;
  DNSServer*     _dns        = nullptr;
  ConfigManager* _cfgMgr     = nullptr;
  UserProfile*   _users      = nullptr;
  int*           _userCount  = nullptr;
  AppConfig*     _cfg        = nullptr;
  unsigned long  _lastScanMs = 0;
  static const unsigned long SCAN_COOLDOWN_MS = 5000UL;
  WeatherManager* _weather     = nullptr;
  EventLogger*    _eventLogger = nullptr;
  TimeManager*    _timeMgr     = nullptr;
  ScaleManager*   _scale       = nullptr;

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

  void setWeatherManager(WeatherManager& wm) { _weather     = &wm; }
  void setEventLogger   (EventLogger&    el) { _eventLogger = &el; }
  void setTimeManager   (TimeManager&    tm) { _timeMgr     = &tm; }
  void setScaleManager  (ScaleManager&   sm) { _scale       = &sm; }

private:
  void _attachRoutes(bool allowScan) {
    _server->on("/",     HTTP_GET,  [this]() { _handleRoot(); });
    if (allowScan) {
      _server->on("/scan", HTTP_GET, [this]() { _handleScan(); });
    } else {
      _server->on("/scan", HTTP_GET, [this]() { _server->send(200, "application/json", "[]"); });
    }
    _server->on("/save",       HTTP_POST, [this]() { _handleSave(); });
    _server->on("/dashboard",  HTTP_GET,  [this]() { _handleDashboard(); });
    _server->on("/api/status", HTTP_GET,  [this]() { _handleApiStatus(); });
    _server->on("/api/events",     HTTP_GET,  [this]() { _handleApiEvents(); });
    _server->on("/api/calibrate",  HTTP_GET,  [this]() { _handleApiCalibrate(); });
    _server->onNotFound(               [this]() { _handleRoot(); });
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
      "<a href=\"/dashboard\" style=\"display:inline-block;margin-bottom:12px;"
        "padding:8px 16px;background:#7c4dff;color:#fff;border-radius:4px;"
        "text-decoration:none;font-size:.9em\">Dashboard &#x5100;&#x8868;&#x677F;</a>\n"
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
      "<label>Calibration Factor<input type=\"number\" step=\"0.01\" name=\"cal_factor\" id=\"cal_factor\" value=\"" + String(_cfg->calibrationFactor, 2) + "\"></label>\n"
      "<div id=\"cal_box\" style=\"margin-top:8px;padding:10px;background:#e3f2fd;border-radius:4px;display:none\">\n"
      "<label style=\"margin-top:0\">現在站上地墊的實際重量 (kg)"
      "<input type=\"number\" step=\"0.1\" id=\"cal_wt\" min=\"5\" max=\"300\" placeholder=\"例如 70.5\"></label>\n"
      "<button type=\"button\" onclick=\"doCalibrate()\" id=\"cal_btn\" style=\"margin-top:8px;width:100%;padding:8px;background:#1565c0;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:.9em\">確認並計算</button>\n"
      "<div id=\"cal_msg\" style=\"margin-top:6px;font-size:.85em\"></div>\n"
      "</div>\n"
      "<button type=\"button\" onclick=\"toggleCal()\" style=\"margin-top:8px;padding:7px 14px;background:#1976d2;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:.85em\">自動計算校正係數</button>\n"
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

      // 自動校正係數
      "function toggleCal(){\n"
      "  var b=document.getElementById('cal_box');\n"
      "  b.style.display=b.style.display==='none'?'block':'none';\n"
      "}\n"
      "function doCalibrate(){\n"
      "  var wt=parseFloat(document.getElementById('cal_wt').value);\n"
      "  var msg=document.getElementById('cal_msg');\n"
      "  var btn=document.getElementById('cal_btn');\n"
      "  if(isNaN(wt)||wt<5){msg.style.color='#c62828';msg.textContent='請輸入有效重量（≥ 5 kg）';return;}\n"
      "  msg.style.color='#555';msg.textContent='讀取感測器中，請站穩...';\n"
      "  btn.disabled=true;\n"
      "  fetch('/api/calibrate?w='+wt).then(function(r){return r.json();}).then(function(d){\n"
      "    if(!d.ok){msg.style.color='#c62828';msg.textContent='錯誤：'+(d.error||'未知');btn.disabled=false;return;}\n"
      "    document.getElementById('cal_factor').value=d.factor.toFixed(2);\n"
      "    msg.style.color='#2e7d32';\n"
      "    msg.textContent='raw='+Math.round(d.raw)+'，新係數='+d.factor.toFixed(2)+'（記得按儲存）';\n"
      "    btn.disabled=false;\n"
      "  }).catch(function(){msg.style.color='#c62828';msg.textContent='連線失敗，請重試';btn.disabled=false;});\n"
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

  void _handleDashboard() {
    static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="zh-TW">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SmartExitMat Dashboard</title>
<style>
:root{--bg:#f5f5f5;--card:#fff;--blue:#2196f3;--green:#4caf50;--red:#f44336;--purple:#7c4dff}
*{box-sizing:border-box}
body{font-family:sans-serif;max-width:520px;margin:20px auto;padding:0 16px;background:var(--bg)}
a.nl{display:inline-block;margin-bottom:12px;padding:8px 16px;background:var(--purple);color:#fff;border-radius:4px;text-decoration:none;font-size:.9em}
h1{font-size:1.3em;color:#333;margin:0 0 12px}
.card{background:var(--card);border-radius:8px;padding:16px;margin-bottom:12px;box-shadow:0 1px 3px rgba(0,0,0,.1)}
.card h2{font-size:1em;margin:0 0 10px;color:#444}
.wt{font-size:2.2em;font-weight:700;color:#333}
.wc{font-size:.85em;color:#888;margin-bottom:4px}
.wa{font-size:.9em;color:#555;white-space:pre-line;margin-top:8px;min-height:1em}
.ug{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:8px}
.ub{padding:10px 12px;border-radius:6px;text-align:center;font-size:.9em;font-weight:500}
.ub.h{background:#e8f5e9;border:2px solid #4caf50;color:#2e7d32}
.ub.o{background:#fce4ec;border:2px solid #f44336;color:#c62828}
.sb{font-size:.75em;color:#aaa;text-align:right;margin-top:8px}
.tabs{display:flex;gap:4px;margin-bottom:10px}
.tab{flex:1;padding:8px;border:none;border-radius:4px;cursor:pointer;background:#e0e0e0;font-size:.9em}
.tab.a{background:var(--blue);color:#fff}
.nr{display:flex;gap:8px;align-items:center;margin-bottom:10px}
.nb{padding:6px 14px;border:1px solid #ccc;border-radius:4px;background:#fff;cursor:pointer}
.nl2{flex:1;text-align:center;font-weight:500;font-size:.95em}
table{width:100%;border-collapse:collapse;font-size:.85em}
th,td{padding:8px 6px;text-align:left;border-bottom:1px solid #eee}
th{background:#f8f8f8;font-weight:600;color:#555}
.eo{color:var(--red);font-weight:500}
.eh{color:var(--green);font-weight:500}
.nd{text-align:center;color:#aaa;padding:16px}
</style>
</head>
<body>
<a class="nl" href="/">&#9881; 設定頁</a>
<h1>SmartExitMat Dashboard</h1>
<div class="card">
<h2>即時天氣</h2>
<div class="wc" id="wc"></div>
<div class="wt" id="wt">--</div>
<div class="wa" id="wa">載入中...</div>
</div>
<div class="card">
<h2>在家狀態</h2>
<div class="ug" id="ug"><div class="nd">載入中...</div></div>
<div class="sb" id="sb">--</div>
</div>
<div class="card">
<h2>事件報表</h2>
<div class="tabs">
<button class="tab a" onclick="sv('day')">日報表</button>
<button class="tab" onclick="sv('week')">週報表</button>
<button class="tab" onclick="sv('month')">月報表</button>
</div>
<div class="nr">
<button class="nb" onclick="nav(-1)">&#8592;</button>
<div class="nl2" id="nl">--</div>
<button class="nb" onclick="nav(1)">&#8594;</button>
</div>
<table>
<thead><tr><th>時間</th><th>使用者</th><th>事件</th><th>重量</th></tr></thead>
<tbody id="eb"></tbody>
</table>
</div>
<script>
var cv='day',rd=new Date();
function p2(n){return n<10?'0'+n:''+n}
function ds(d){return d.getFullYear()+'-'+p2(d.getMonth()+1)+'-'+p2(d.getDate())}
function ft(ts){var d=new Date(ts*1000);return p2(d.getMonth()+1)+'/'+p2(d.getDate())+' '+p2(d.getHours())+':'+p2(d.getMinutes())}
function mon(d){var c=new Date(d),dy=c.getDay()||7;c.setDate(c.getDate()-dy+1);return c}
function ul(){
var s='';
if(cv==='day'){s=ds(rd);}
else if(cv==='week'){var m=mon(rd),e=new Date(m);e.setDate(m.getDate()+6);s=ds(m)+'~'+ds(e);}
else{s=rd.getFullYear()+'年'+(rd.getMonth()+1)+'月';}
document.getElementById('nl').textContent=s;
}
function sv(v){cv=v;rd=new Date();document.querySelectorAll('.tab').forEach(function(b,i){b.classList.toggle('a',['day','week','month'][i]===v)});ul();le();}
function esc(s){var d=document.createElement('div');d.textContent=s;return d.innerHTML}
function nav(d){
if(cv==='day')rd.setDate(rd.getDate()+d);
else if(cv==='week')rd.setDate(rd.getDate()+d*7);
else rd.setMonth(rd.getMonth()+d);
ul();le();
}
function ls(){
fetch('/api/status').then(function(r){return r.json();}).then(function(d){
var w=d.weather||{};
document.getElementById('wt').textContent=w.has_data?(w.temp_c.toFixed(1)+'°C'):'--';
document.getElementById('wc').textContent=w.city||'';
document.getElementById('wa').textContent=w.has_data?(w.advisory||'天氣正常'):'天氣資料未取得';
var ug=document.getElementById('ug');
var us=d.users||[];
if(!us.length){ug.innerHTML='<div class="nd">尚未設定使用者</div>';return;}
ug.innerHTML='';
us.forEach(function(u){var div=document.createElement('div');div.className='ub '+(u.at_home?'h':'o');div.innerHTML='<div>'+esc(u.name)+'</div><div style="font-size:.8em;margin-top:3px">'+(u.at_home?'在家':'外出')+'</div>';ug.appendChild(div);});
document.getElementById('sb').textContent='更新：'+(d.time||'--');
}).catch(function(){document.getElementById('wa').textContent='無法連線至裝置';});}
function le(){
var url='/api/events?view='+cv;
if(cv==='day')url+='&date='+ds(rd);
else if(cv==='week')url+='&date='+ds(mon(rd));
else url+='&year='+rd.getFullYear()+'&month='+(rd.getMonth()+1);
fetch(url).then(function(r){return r.json();}).then(function(d){
var tb=document.getElementById('eb'),evs=d.events||[];
if(!evs.length){tb.innerHTML='<tr><td colspan="4" class="nd">此期間無紀錄</td></tr>';return;}
tb.innerHTML='';
evs.forEach(function(ev){var tr=document.createElement('tr');var io=ev.ev==='out';tr.innerHTML='<td>'+ft(ev.ts)+'</td><td>'+esc(ev.nm)+'</td><td class="'+(io?'eo':'eh')+'">'+(io?'出門':'回家')+'</td><td>'+ev.kg.toFixed(1)+' kg</td>';tb.appendChild(tr);});
}).catch(function(){document.getElementById('eb').innerHTML='<tr><td colspan="4" style="color:#f44336;text-align:center">載入失敗</td></tr>';});}
ls();ul();le();
setInterval(ls,30000);
</script>
</body>
</html>)rawliteral";
    _server->send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
  }

  void _handleApiStatus() {
    String json = "{\"weather\":{";
    if (_weather && _weather->hasData()) {
      char tbuf[10];
      snprintf(tbuf, sizeof(tbuf), "%.1f", _weather->temperature());
      json += "\"has_data\":true,\"temp_c\":";
      json += tbuf;
      snprintf(tbuf, sizeof(tbuf), "%.1f", _weather->feelsLike());
      json += ",\"feels_like_c\":";
      json += tbuf;
      json += ",\"advisory\":\"";
      json += _escapeJson(_weather->advisory());
      json += "\",\"city\":\"";
      json += _cfg ? _escapeJson(_cfg->owmCity) : "";
      json += "\"";
    } else {
      json += "\"has_data\":false";
    }
    json += "},\"users\":[";
    int cnt = _userCount ? *_userCount : 0;
    for (int i = 0; i < cnt; i++) {
      if (i > 0) json += ",";
      json += "{\"id\":\"";
      json += _users[i].id;
      json += "\",\"name\":\"";
      json += _escapeJson(_users[i].name);
      json += "\",\"at_home\":";
      json += _users[i].atHome ? "true" : "false";
      json += "}";
    }
    String t = _timeMgr ? _timeMgr->getCurrentTimeString() : String("");
    json += "],\"time\":\"";
    json += _escapeJson(t);
    json += "\"}";
    _server->send(200, "application/json", json);
  }

  void _handleApiEvents() {
    if (!_eventLogger) {
      _server->send(503, "application/json", "{\"error\":\"logger not ready\"}");
      return;
    }
    String view  = _server->hasArg("view")  ? _server->arg("view")  : "day";
    if (view != "day" && view != "week" && view != "month") view = "day";
    String date  = _server->hasArg("date")  ? _server->arg("date")  : "";
    int    year  = _server->hasArg("year")  ? _server->arg("year").toInt()  : 0;
    int    month = _server->hasArg("month") ? _server->arg("month").toInt() : 0;
    // Derive year/month from date string when not supplied
    if ((year == 0 || month == 0) && date.length() >= 7) {
      year  = date.substring(0, 4).toInt();
      month = date.substring(5, 7).toInt();
    }
    // Final fallback to current time
    if (year == 0 || month == 0) {
      time_t now = time(nullptr);
      struct tm t;
      localtime_r(&now, &t);
      year  = t.tm_year + 1900;
      month = t.tm_mon + 1;
    }
    String json = _eventLogger->getEventsJson(year, month, view, date);
    _server->send(200, "application/json", json);
  }

  void _handleApiCalibrate() {
    if (!_scale) {
      _server->send(503, "application/json", "{\"ok\":false,\"error\":\"scale not initialized\"}");
      return;
    }
    if (!_server->hasArg("w")) {
      _server->send(400, "application/json", "{\"ok\":false,\"error\":\"missing param w\"}");
      return;
    }
    float knownWeight = _server->arg("w").toFloat();
    if (knownWeight < 5.0f) {
      _server->send(400, "application/json", "{\"ok\":false,\"error\":\"weight must be >= 5 kg\"}");
      return;
    }
    double rawVal = _scale->getRawValue(5);
    if (rawVal < -10000.0) {
      char buf[80];
      snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"wiring reversed? raw=%.0f\"}", rawVal);
      _server->send(400, "application/json", buf);
      return;
    }
    if (rawVal < 10000.0) {
      _server->send(400, "application/json",
        "{\"ok\":false,\"error\":\"no weight detected — please stand on the mat\"}");
      return;
    }
    float factor = (float)(rawVal / knownWeight);
    if (!isfinite(factor) || factor <= 0.0f) {
      _server->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid factor\"}");
      return;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"ok\":true,\"raw\":%.0f,\"factor\":%.2f}", rawVal, factor);
    _server->send(200, "application/json", buf);
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
