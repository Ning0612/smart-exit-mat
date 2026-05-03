#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "AppTypes.h"

class EventLogger {
  bool _ready = false;

  String _filePath(int year, int month) {
    char buf[28];
    snprintf(buf, sizeof(buf), "/events/%04d-%02d.jsonl", year, month);
    return String(buf);
  }

  // Append lines from path whose "ts" falls in [ts_start, ts_end) to result.
  // Stops once maxEvents lines have been appended (heap protection).
  int _readFileInRange(const String& path, time_t ts_start, time_t ts_end,
                       String& result, bool& first, int maxEvents) {
    if (maxEvents <= 0 || !LittleFS.exists(path)) return 0;
    File f = LittleFS.open(path, FILE_READ);
    if (!f) return 0;
    int count = 0;
    while (f.available() && count < maxEvents) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.isEmpty()) continue;
      // Quick extraction of "ts" value without full JSON parse
      int idx = line.indexOf("\"ts\":");
      if (idx < 0) continue;
      idx += 5;
      int end = idx;
      while (end < (int)line.length() && (line[end] == '-' || isDigit(line[end]))) end++;
      time_t ts = (time_t)line.substring(idx, end).toInt();
      if (ts >= ts_start && ts < ts_end) {
        if (!first) result += ',';
        first = false;
        result += line;
        count++;
      }
    }
    f.close();
    return count;
  }

public:
  bool begin() {
    if (!LittleFS.begin(true, "/littlefs", 5, "spiffs")) {
      Serial.println("[EventLogger] LittleFS mount failed");
      _ready = false;
      return false;
    }
    LittleFS.mkdir("/events");
    Serial.println("[EventLogger] ready");
    _ready = true;
    return true;
  }

  bool log(const EventRecord& rec) {
    if (!_ready || (unsigned long)rec.ts < 1000000000UL) return false;
    struct tm t;
    localtime_r(&rec.ts, &t);
    String path = _filePath(t.tm_year + 1900, t.tm_mon + 1);

    JsonDocument doc;
    doc["ts"]  = (long)rec.ts;
    doc["uid"] = rec.uid;
    doc["nm"]  = rec.name;
    doc["ev"]  = rec.evType;
    char kgbuf[8];
    snprintf(kgbuf, sizeof(kgbuf), "%.1f", rec.kg);
    doc["kg"]  = serialized(kgbuf);

    String line;
    serializeJson(doc, line);

    File f = LittleFS.open(path, FILE_APPEND);
    if (!f) return false;
    f.println(line);
    f.close();
    Serial.printf("[EventLogger] %s %s\n", rec.name.c_str(), rec.evType.c_str());
    return true;
  }

  // view: "day" | "week" | "month"
  // refDate: "YYYY-MM-DD" used for day/week; empty ok for month
  // year/month: used for month view (also derived from refDate as fallback)
  String getEventsJson(int year, int month,
                       const String& view, const String& refDate) {
    if (!_ready) return "{\"events\":[],\"count\":0}";

    time_t ts_start = 0, ts_end = 0;

    if (view == "day") {
      if (refDate.length() >= 10) {
        struct tm t = {};
        t.tm_year  = refDate.substring(0, 4).toInt() - 1900;
        t.tm_mon   = refDate.substring(5, 7).toInt() - 1;
        t.tm_mday  = refDate.substring(8, 10).toInt();
        t.tm_isdst = -1;
        ts_start = mktime(&t);
        ts_end   = ts_start + 86400L;
      }
    } else if (view == "week") {
      // refDate is always the Monday of the week (from dashboard JS)
      if (refDate.length() >= 10) {
        struct tm t = {};
        t.tm_year  = refDate.substring(0, 4).toInt() - 1900;
        t.tm_mon   = refDate.substring(5, 7).toInt() - 1;
        t.tm_mday  = refDate.substring(8, 10).toInt();
        t.tm_isdst = -1;
        ts_start = mktime(&t);
        ts_end   = ts_start + 7L * 86400L;
      }
    } else {  // "month"
      if (year == 0 || month == 0) {
        time_t now = time(nullptr);
        struct tm tnow;
        localtime_r(&now, &tnow);
        year  = tnow.tm_year + 1900;
        month = tnow.tm_mon + 1;
      }
      struct tm t = {};
      t.tm_year  = year - 1900;
      t.tm_mon   = month - 1;
      t.tm_mday  = 1;
      t.tm_isdst = -1;
      ts_start = mktime(&t);
      t.tm_mon++;
      if (t.tm_mon == 12) { t.tm_mon = 0; t.tm_year++; }
      ts_end = mktime(&t);
    }

    if (ts_start == 0) return "{\"events\":[],\"count\":0}";

    // Determine which month files to read (week may span two months)
    struct tm t_s;
    localtime_r(&ts_start, &t_s);
    int y1 = t_s.tm_year + 1900, m1 = t_s.tm_mon + 1;

    time_t ts_e1 = ts_end - 1;
    struct tm t_e;
    localtime_r(&ts_e1, &t_e);
    int y2 = t_e.tm_year + 1900, m2 = t_e.tm_mon + 1;

    String result = "{\"view\":\"";
    result += view;
    result += "\",\"events\":[";
    bool first = true;
    int count = 0;

    const int MAX_EVENTS = 300;
    count += _readFileInRange(_filePath(y1, m1), ts_start, ts_end, result, first, MAX_EVENTS);
    if (y1 != y2 || m1 != m2) {
      count += _readFileInRange(_filePath(y2, m2), ts_start, ts_end, result, first, MAX_EVENTS - count);
    }

    result += "],\"count\":";
    result += String(count);
    result += "}";
    return result;
  }

  void pruneOldFiles(int keepMonths = 6) {
    if (!_ready) return;
    time_t now = time(nullptr);
    if ((unsigned long)now < 1000000000UL) return;  // NTP not yet synced

    struct tm t;
    localtime_r(&now, &t);
    int limitYM = (t.tm_year + 1900) * 12 + (t.tm_mon + 1) - keepMonths;

    File dir = LittleFS.open("/events");
    if (!dir || !dir.isDirectory()) return;

    // Collect paths first to avoid modifying directory while iterating
    String toPrune[20];
    int pruneCount = 0;

    File f = dir.openNextFile();
    while (f && pruneCount < 20) {
      String fullPath = String(f.name());
      f.close();
      int slash = fullPath.lastIndexOf('/');
      String fname = (slash >= 0) ? fullPath.substring(slash + 1) : fullPath;
      if (fname.length() >= 12 && fname.endsWith(".jsonl")) {
        int y = fname.substring(0, 4).toInt();
        int m = fname.substring(5, 7).toInt();
        if (y > 1970 && m >= 1 && m <= 12 && y * 12 + m < limitYM) {
          // Reconstruct absolute path
          toPrune[pruneCount++] = (fullPath.startsWith("/")) ? fullPath : "/events/" + fname;
        }
      }
      f = dir.openNextFile();
    }
    dir.close();

    for (int i = 0; i < pruneCount; i++) {
      LittleFS.remove(toPrune[i]);
      Serial.printf("[EventLogger] pruned %s\n", toPrune[i].c_str());
    }
  }
};
