#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class LineNotifier {
public:
  bool send(const String& token, const String& toId,
            const String& name, const String& eventType,
            const String& timestamp, float weightKg,
            const String& weatherAdvisory = "") {

    String text = name + " " + eventType
                + "\n時間：" + timestamp
                + "\n偵測重量：" + String(weightKg, 1) + " kg";
    if (!weatherAdvisory.isEmpty()) {
      text += "\n\n天氣提醒：\n" + weatherAdvisory;
    }
    return _pushText(token, toId, text);
  }

  bool sendText(const String& token, const String& toId, const String& text) {
    if (token.isEmpty() || toId.isEmpty()) return false;
    return _pushText(token, toId, text);
  }

private:
  bool _pushText(const String& token, const String& toId, const String& text) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    if (!https.begin(client, "https://api.line.me/v2/bot/message/push")) {
      Serial.println("[LINE] begin() failed");
      return false;
    }

    https.addHeader("Content-Type",  "application/json");
    https.addHeader("Authorization", "Bearer " + token);

    String body = "{\"to\":\"" + _escapeJson(toId) + "\","
                  "\"messages\":[{\"type\":\"text\",\"text\":\""
                  + _escapeJson(text) + "\"}]}";

    int code = https.POST(body);
    https.end();

    if (code == HTTP_CODE_OK || code == HTTP_CODE_NO_CONTENT) {
      Serial.printf("[LINE] sent OK (%d)\n", code);
      return true;
    }
    Serial.printf("[LINE] HTTP error %d\n", code);
    return false;
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
};
