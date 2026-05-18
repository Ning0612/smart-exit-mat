#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "RootCerts.h"

class WeatherManager {
  String        _advisory;
  bool          _hasRainAdvisory = false;
  bool          _tempAdvised     = false;   // reset in _fetch; set by _addClearSky to skip duplicate
  unsigned long _lastFetchMs     = 0;
  bool          _lastOk          = false;
  float         _tempC           = NAN;
  float         _feelsLikeC      = NAN;
  int           _humidity        = -1;      // %, -1 = no data
  float         _windSpeed       = NAN;     // m/s

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
  int           humidity()    const { return _humidity; }
  float         windSpeed()   const { return _windSpeed; }
  bool          hasData()     const { return _lastOk && !isnan(_tempC); }

private:
  bool _fetch(const String& apiKey, const String& city) {
    _advisory        = "";
    _hasRainAdvisory = false;
    _tempAdvised     = false;
    _tempC           = NAN;
    _feelsLikeC      = NAN;
    _humidity        = -1;
    _windSpeed       = NAN;
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
        filter["main"]["humidity"]   = true;
        filter["wind"]["speed"]      = true;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body,
          DeserializationOption::Filter(filter));
        if (!err) {
          int   humid = doc["main"]["humidity"] | -1;
          float wind  = doc["wind"]["speed"]    | -1.0f;
          _buildCurrentAdvisory(
            doc["weather"][0]["id"]   | 800,
            doc["main"]["temp"]       | 25.0f,
            doc["main"]["feels_like"] | 25.0f,
            humid,
            wind >= 0.0f ? wind : NAN);
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

  void _buildCurrentAdvisory(int id, float temp, float feelsLike, int humidity, float windSpeed) {
    _tempC      = temp;
    _feelsLikeC = feelsLike;
    _humidity   = humidity;
    _windSpeed  = windSpeed;

    if      (id >= 200 && id < 300) { _addThunderstorm(id); _hasRainAdvisory = true; }
    else if (id >= 300 && id < 400) { _addDrizzle(id);      _hasRainAdvisory = true; }
    else if (id >= 500 && id < 600) { _addRain(id);         _hasRainAdvisory = true; }
    else if (id >= 600 && id < 700)   _addSnow(id);
    else if (id >= 700 && id < 800)   _addAtmosphere(id);
    else if (id == 800)               _addClearSky(temp, feelsLike);
    else if (id > 800 && id < 805)    _addCloudy(id);

    _addTemperatureAdvisory(temp, feelsLike);

    if (humidity >= 0) {
      if      (humidity >= 85 && temp > 26.0f) _add("高溫高濕悶熱（濕度 " + String(humidity) + "%），外出請多補水");
      else if (humidity <= 30)                  _add("空氣乾燥（濕度 " + String(humidity) + "%），建議多補充水分");
    }

    if (!isnan(windSpeed)) {
      if      (windSpeed >= 24.0f) _add("暴風（風速 " + String((int)windSpeed) + " m/s），非必要請勿外出");
      else if (windSpeed >= 17.0f) _add("強風（" + String((int)windSpeed) + " m/s），外出請注意安全");
      else if (windSpeed >= 10.0f) _add("風速較強（" + String((int)windSpeed) + " m/s），騎車請特別小心");
    }
  }

  void _addThunderstorm(int id) {
    switch (id) {
      case 200: _add("有雷雨（含閃電），出門注意安全");       break;
      case 201: _add("雷雨較大，建議延後外出");               break;
      case 202: _add("劇烈雷暴雨，非必要請勿外出");           break;
      case 210: _add("有輕雷，注意天氣變化");                 break;
      case 211: _add("打雷中，避免在空曠處停留");             break;
      case 212: _add("強烈雷暴，請找室內避雷");               break;
      case 221: _add("不規律雷暴，隨時注意天氣動態");         break;
      case 230: _add("毛毛雨夾雷，帶傘並注意安全");           break;
      case 231: _add("雷陣雨，帶傘小心出行");                 break;
      case 232: _add("大雷陣雨，謹慎出行");                   break;
      default:  _add("雷暴天氣，請注意安全並帶傘");           break;
    }
  }

  void _addDrizzle(int id) {
    switch (id) {
      case 300: _add("有輕微毛毛雨，備好雨傘");   break;
      case 301: _add("毛毛雨中，建議帶傘出門");   break;
      case 302: _add("毛毛雨偏大，記得帶傘");     break;
      case 310: _add("有零星飄雨，備好雨具");     break;
      case 311: _add("細雨飄飄，帶傘較舒適");     break;
      case 312: _add("細雨中等，建議帶傘");       break;
      case 313: _add("陣雨夾細雨，請帶傘備用");   break;
      case 314: _add("陣性大雨，請帶傘出行");     break;
      case 321: _add("零星陣雨，備好雨傘");       break;
      default:  _add("有毛毛雨，記得帶傘");       break;
    }
  }

  void _addRain(int id) {
    switch (id) {
      case 500: _add("小雨，帶傘出門不淋濕");             break;
      case 501: _add("中雨，記得帶傘");                   break;
      case 502: _add("大雨，請備好雨具並謹慎出行");       break;
      case 503: _add("暴雨，外出請特別注意安全");         break;
      case 504: _add("極端暴雨，非必要請留在室內");       break;
      case 511: _add("有凍雨，路面可能結冰，請小心行走"); break;
      case 520: _add("陣雨，帶傘備用");                   break;
      case 521: _add("中等陣雨，請帶傘出行");             break;
      case 522: _add("大型陣雨，請備好雨具");             break;
      case 531: _add("不規律陣雨，建議攜帶雨傘");         break;
      default:  _add("正在下雨，記得帶傘");               break;
    }
  }

  void _addSnow(int id) {
    switch (id) {
      case 600: _add("有小雪，注意保暖及路面濕滑");           break;
      case 601: _add("正在下雪，請穿著保暖並注意行走安全");   break;
      case 602: _add("大雪，請做好保暖並注意路滑");           break;
      case 611: _add("有雨夾雪，路面可能結冰，請小心行走");   break;
      case 612: _add("有輕微雨夾雪，請注意路況");             break;
      case 613: _add("陣性雨夾雪，行走需小心");               break;
      case 615: _add("小雪夾雨，保暖並注意路滑");             break;
      case 616: _add("雪雨混合，路況不佳請謹慎");             break;
      case 620: _add("有陣雪，注意防滑保暖");                 break;
      case 621: _add("陣雪中，行走需特別小心");               break;
      case 622: _add("大型陣雪，保暖防滑為首要");             break;
      default:  _add("有降雪，注意保暖防滑");                 break;
    }
  }

  void _addAtmosphere(int id) {
    switch (id) {
      // 能見度 / 空氣品質類
      case 701: _add("有薄霧，行車請保持安全距離");           break;
      case 711: _add("空氣中有煙霧，建議戴口罩出行");         break;
      case 721: _add("有霾，空氣品質不佳，建議戴口罩");       break;
      case 731: _add("有沙塵旋風，請注意眼睛防護");           break;
      case 741: _add("濃霧，能見度低，行車請開霧燈並慢行");   break;
      case 751: _add("空氣中有沙塵，建議戴口罩並保護眼睛");   break;
      case 761: _add("灰塵瀰漫，建議戴口罩外出");             break;
      case 762: _add("空氣中有火山灰，請戴口罩並減少外出");   break;
      // 強風 / 災害類
      case 771: _add("有強陣風，外出注意安全");               break;
      case 781: _add("龍捲風警告，請立即至室內避難");         break;
      default:  _add("能見度不佳，外出請注意安全");           break;
    }
  }

  void _addClearSky(float temp, float feelsLike) {
    if      (temp >= 32.0f) _add("豔陽高照（" + String((int)temp) + "°C），做好防曬並補水");
    else if (temp >= 25.0f) _add("天氣晴朗（" + String((int)temp) + "°C），記得擦防曬乳");
    else if (temp >= 18.0f) _add("晴空萬里，天氣舒適（" + String((int)temp) + "°C），適合外出");
    else if (temp >= 10.0f) _add("陽光普照但偏涼（" + String((int)temp) + "°C），建議帶件外套");
    else                     _add("晴天但氣溫低（" + String((int)temp) + "°C），請注意保暖");
    _tempAdvised = true;
  }

  void _addCloudy(int id) {
    switch (id) {
      case 801: _add("晴時多雲，大致天氣尚好");   break;
      case 802: _add("部分多雲，天氣尚佳");       break;
      case 803: _add("多雲，偶有陽光");           break;
      case 804: _add("陰天，有可能轉雨，可備傘"); break;
      default:  break;
    }
  }

  void _addTemperatureAdvisory(float temp, float feelsLike) {
    if (_tempAdvised) return;
    if      (temp < 0.0f)        _add("氣溫零下（" + String((int)temp) + "°C），請穿厚重大衣、手套與帽子");
    else if (temp < 5.0f)        _add("氣溫極低（" + String((int)temp) + "°C），外出務必穿保暖大衣");
    else if (temp < 10.0f)       _add("天氣很冷（" + String((int)temp) + "°C），建議穿外套保暖");
    else if (temp < 15.0f)       _add("天氣偏涼（" + String((int)temp) + "°C），多穿一層較舒適");
    else if (temp < 20.0f)       _add("天氣微涼（" + String((int)temp) + "°C），可帶件薄外套備用");
    else if (feelsLike >= 40.0f) _add("體感酷熱（" + String((int)feelsLike) + "°C），盡量減少戶外活動時間");
    else if (feelsLike >= 35.0f) _add("體感炎熱（" + String((int)feelsLike) + "°C），注意防曬與補水");
  }

  void _buildForecastAdvisory(JsonArrayConst list) {
    float maxPop   = 0.0f;
    bool  willRain = false;
    bool  thunder  = false;
    bool  willSnow = false;

    for (JsonVariantConst v : list) {
      JsonObjectConst item = v.as<JsonObjectConst>();
      float pop = item["pop"] | 0.0f;
      if (pop > maxPop) maxPop = pop;
      int id = item["weather"][0]["id"] | 800;
      if (id >= 200 && id < 300 && pop > 0.3f) thunder  = true;
      if (id >= 200 && id < 600 && pop > 0.3f) willRain = true;
      if (id >= 600 && id < 700 && pop > 0.3f) willSnow = true;
    }

    // 降雪預報重要性高，即使當前已有降水 advisory 仍追加提示
    if (willSnow) {
      _add(_hasRainAdvisory
        ? "注意：今日稍晚可能降雪，請留意路況"
        : "12 小時內可能降雪，注意保暖與路況");
      return;
    }

    if (_hasRainAdvisory) return;

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

  String _urlEncode(const String& s) {
    String out;
    out.reserve(s.length() * 3);
    for (unsigned int i = 0; i < s.length(); i++) {
      unsigned char c = (unsigned char)s[i];
      if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_') {
        out += (char)c;
      } else {
        char buf[4];
        snprintf(buf, sizeof(buf), "%%%02X", c);
        out += buf;
      }
    }
    return out;
  }

  String _httpGet(const String& url) {
    WiFiClientSecure client;
    client.setCACert(OWM_ROOT_CA);
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
