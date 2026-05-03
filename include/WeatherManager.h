#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

class WeatherManager {
  String        _advisory;
  bool          _hasRainAdvisory = false;
  unsigned long _lastFetchMs     = 0;
  bool          _lastOk          = false;
  float         _tempC           = NAN;
  float         _feelsLikeC      = NAN;

  static const unsigned long REFRESH_MS = 30UL * 60UL * 1000UL;
  static const unsigned long RETRY_MS   =  5UL * 60UL * 1000UL;

public:
  void updateIfNeeded(const String& apiKey, const String& city) {
    if (apiKey.isEmpty() || city.isEmpty()) return;
    if (WiFi.status() != WL_CONNECTED) return;
    unsigned long interval = _lastOk ? REFRESH_MS : RETRY_MS;
    if (_lastFetchMs != 0 && millis() - _lastFetchMs < interval) return;
    _lastOk      = _fetch(apiKey, city);
    _lastFetchMs = millis();
  }

  const String& advisory()    const { return _advisory; }
  float         temperature() const { return _tempC; }
  float         feelsLike()   const { return _feelsLikeC; }
  bool          hasData()     const { return _lastOk && !isnan(_tempC); }

private:
  bool _fetch(const String& apiKey, const String& city) {
    _advisory        = "";
    _hasRainAdvisory = false;
    _tempC           = NAN;
    _feelsLikeC      = NAN;
    bool ok          = false;
    String cityEnc   = _urlEncode(city);

    // 當前天氣
    {
      String body = _httpGet(
        "https://api.openweathermap.org/data/2.5/weather?q="
        + cityEnc + ",TW&appid=" + apiKey + "&units=metric");
      if (!body.isEmpty()) {
        JsonDocument filter;
        filter["weather"][0]["id"]   = true;
        filter["main"]["temp"]       = true;
        filter["main"]["feels_like"] = true;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body,
          DeserializationOption::Filter(filter));
        if (!err) {
          _buildCurrentAdvisory(
            doc["weather"][0]["id"]   | 800,
            doc["main"]["temp"]       | 25.0f,
            doc["main"]["feels_like"] | 25.0f);
          ok = true;
        } else {
          Serial.printf("[Weather] parse error: %s\n", err.c_str());
        }
      }
    }

    // 12 小時預報（4 * 3h）
    {
      String body = _httpGet(
        "https://api.openweathermap.org/data/2.5/forecast?q="
        + cityEnc + ",TW&appid=" + apiKey + "&units=metric&cnt=4");
      if (!body.isEmpty()) {
        JsonDocument filter;
        filter["list"][0]["weather"][0]["id"] = true;
        filter["list"][0]["pop"]              = true;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body,
          DeserializationOption::Filter(filter));
        if (!err) {
          _buildForecastAdvisory(doc["list"].as<JsonArrayConst>());
        } else {
          Serial.printf("[Weather] forecast parse error: %s\n", err.c_str());
        }
      }
    }

    Serial.printf("[Weather] advisory: %s\n",
      _advisory.isEmpty() ? "(none)" : _advisory.c_str());
    return ok;
  }

  void _buildCurrentAdvisory(int id, float temp, float feelsLike) {
    _tempC      = temp;
    _feelsLikeC = feelsLike;
    if (id >= 200 && id < 300) {
      _add("正在打雷閃電，請注意安全");
      _hasRainAdvisory = true;
    } else if (id >= 300 && id < 400) {
      _add("正在飄毛毛雨，記得帶傘");
      _hasRainAdvisory = true;
    } else if (id >= 500 && id < 600) {
      _add("正在下雨，記得帶傘");
      _hasRainAdvisory = true;
    } else if (id >= 600 && id < 700) {
      _add("正在下雪，注意保暖");
    }

    if      (temp < 12.0f)      _add("天氣寒冷（" + String((int)temp) + "C），記得穿外套保暖");
    else if (temp < 16.0f)      _add("天氣涼（"   + String((int)temp) + "C），建議多穿一件");
    else if (feelsLike > 35.0f) _add("天氣炎熱（體感 " + String((int)feelsLike) + "C），注意防曬補水");
  }

  void _buildForecastAdvisory(JsonArrayConst list) {
    if (_hasRainAdvisory) return;

    float maxPop   = 0.0f;
    bool  willRain = false;
    bool  thunder  = false;

    for (JsonVariantConst v : list) {
      JsonObjectConst item = v.as<JsonObjectConst>();
      float pop = item["pop"] | 0.0f;
      if (pop > maxPop) maxPop = pop;
      int id = item["weather"][0]["id"] | 800;
      if (id >= 200 && id < 300 && pop > 0.3f) thunder  = true;
      if (id >= 200 && id < 600 && pop > 0.3f) willRain = true;
    }

    if (thunder) {
      _add("12 小時內可能有雷雨，謹慎出行");
    } else if (willRain || maxPop > 0.4f) {
      _add("待會可能下雨（機率 " + String((int)(maxPop * 100)) + "%），建議帶傘");
    }
  }

  void _add(const String& s) {
    if (!_advisory.isEmpty()) _advisory += '\n';
    _advisory += s;
  }

  // 僅對空白做 %20 轉換，城市名稱一般不含其他特殊字元
  String _urlEncode(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (unsigned int i = 0; i < s.length(); i++) {
      char c = s[i];
      if (c == ' ') out += "%20";
      else          out += c;
    }
    return out;
  }

  String _httpGet(const String& url) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    if (!https.begin(client, url)) return "";
    int    code = https.GET();
    String body;
    if (code == HTTP_CODE_OK) {
      body = https.getString();
    } else {
      Serial.printf("[Weather] HTTP %d\n", code);
    }
    https.end();
    return body;
  }
};
