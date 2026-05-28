#include "JinaSearch.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

String jina_search(const String& query, int max_results) {
  // URL-encode query
  String url = "https://s.jina.ai/?q=";
  for (size_t i = 0; i < query.length(); i++) {
    char c = query.charAt(i);
    if (isalnum((unsigned char)c)) {
      url += c;
    } else {
      char buf[5];
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      url += buf;
    }
  }

  Serial.printf("[jina] GET %s\n", url.c_str());
  HTTPClient http;
  http.setTimeout(10000);
  http.setReuse(false);
  http.begin(url);
  http.addHeader("Accept", "application/json");

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[jina] HTTP %d\n", code);
    http.end();
    return String("検索に失敗しました（HTTP ") + code + "）。";
  }

  String body = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[jina] json parse error: %s\n", err.c_str());
    return "検索結果の解析に失敗しました。";
  }

  JsonArray data = doc["data"].as<JsonArray>();
  if (data.isNull() || data.size() == 0) {
    return "検索結果が見つかりませんでした。";
  }

  String out = "";
  int count = 0;
  for (JsonObject item : data) {
    if (count >= max_results) break;
    String title = item["title"] | "（タイトルなし）";
    String desc  = item["description"] | item["content"] | "";
    if (desc.length() > 150) desc = desc.substring(0, 150) + "…";
    out += String(count + 1) + ". " + title;
    if (desc.length() > 0) out += " — " + desc;
    out += "\n";
    count++;
  }

  return out;
}
