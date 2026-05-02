#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

class LineNotifier {
public:
  bool send(const String& token, const String& toId,
            const String& name, const String& eventType,
            const String& timestamp, float weightKg) {

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    if (!https.begin(client, "https://api.line.me/v2/bot/message/push")) {
      Serial.println("[LINE] begin() failed");
      return false;
    }

    https.addHeader("Content-Type",  "application/json");
    https.addHeader("Authorization", "Bearer " + token);

    String text = name + " " + eventType
                + "\n時間：" + timestamp
                + "\n偵測重量：" + String(weightKg, 1) + " kg";

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

private:
  String _escapeJson(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    for (unsigned int i = 0; i < s.length(); i++) {
      char c = s[i];
      if      (c == '"')  out += "\\\"";
      else if (c == '\\') out += "\\\\";
      else if (c == '\n') out += "\\n";
      else if (c == '\r') out += "\\r";
      else                out += c;
    }
    return out;
  }
};
