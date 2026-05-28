#include "DateTimeTool.h"
#include <Arduino.h>
#include <time.h>

const char* GetDateTool::schema_json() const {
  return
    "{"
      "\"name\": \"get_date\","
      "\"description\": \"今日の日付を取得する。\","
      "\"parameters\": {"
        "\"type\":\"object\","
        "\"properties\": {}"
      "}"
    "}";
}

String GetDateTool::execute(JsonObject /*args*/) {
  struct tm timeInfo;
  if (getLocalTime(&timeInfo)) {
    return String(timeInfo.tm_year + 1900) + "年"
         + String(timeInfo.tm_mon + 1) + "月"
         + String(timeInfo.tm_mday) + "日";
  }
  return "時刻取得に失敗しました。";
}


const char* GetTimeTool::schema_json() const {
  return
    "{"
      "\"name\": \"get_time\","
      "\"description\": \"現在の時刻を取得する。\","
      "\"parameters\": {"
        "\"type\":\"object\","
        "\"properties\": {}"
      "}"
    "}";
}

String GetTimeTool::execute(JsonObject /*args*/) {
  struct tm timeInfo;
  if (getLocalTime(&timeInfo)) {
    return String(timeInfo.tm_hour) + "時" + String(timeInfo.tm_min) + "分";
  }
  return "時刻取得に失敗しました。";
}


const char* GetWeekTool::schema_json() const {
  return
    "{"
      "\"name\": \"get_week\","
      "\"description\": \"今日が何曜日かを取得する。\","
      "\"parameters\": {"
        "\"type\":\"object\","
        "\"properties\": {}"
      "}"
    "}";
}

String GetWeekTool::execute(JsonObject /*args*/) {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo)) {
    return "時刻取得に失敗しました。";
  }
  static const char* names[] = {"日曜日", "月曜日", "火曜日", "水曜日", "木曜日", "金曜日", "土曜日"};
  int w = timeInfo.tm_wday;
  if (w < 0 || w > 6) return "（不明）";
  return String(names[w]);
}
