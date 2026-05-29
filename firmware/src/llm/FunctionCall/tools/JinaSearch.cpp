#include "JinaSearch.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "../../../SpiRamJsonDocument.h"

// composition root から注入される。未設定の場合は Authorization ヘッダを
// 付けず（呼ぶ意味はないが）、s.jina.ai から 401 を受け取る。
static String s_jina_api_key = "";

void jina_set_api_key(const String& key) {
  s_jina_api_key = key;
  Serial.printf("[jina] api_key set (len=%d)\n", (int)key.length());
}

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

  // ChatGPT.cpp と同じ HTTPS パターン:
  //   WiFiClientSecure を自前で持ち、TLS handshake を制御する。
  //   素の http.begin(url) では handshake/読み込みが遅くタイムアウト (-11) する。
  //   Jina は Cloudflare 経由でリージョン毎に CDN/CA が変わるため、setInsecure()
  //   で証明書検証をスキップする（公開検索 API なので機密性は低い）。
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(30000);  // 検索＋整形に時間がかかるので 30 秒
  http.setReuse(false);
  if (!http.begin(client, url)) {
    Serial.println("[jina] http.begin failed");
    return "検索サービスへの接続に失敗しました。";
  }
  http.addHeader("Accept", "application/json");
  // Jina はデフォルトで検索結果ページの本文 HTML を content に入れて返す。
  // ESP32 では 300KB+ になり JSON パース不能。no-content で省略させる。
  // → title / url / description のみ返るのでレスポンスは数 KB に縮む。
  http.addHeader("X-Respond-With", "no-content");
  if (s_jina_api_key.length() > 0) {
    http.addHeader("Authorization", String("Bearer ") + s_jina_api_key);
  }

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[jina] HTTP %d\n", code);
    http.end();
    if (code == 401 || code == 403) {
      return "検索サービスの認証に失敗しました。APIキーを確認してください。";
    }
    if (code == HTTPC_ERROR_READ_TIMEOUT || code == HTTPC_ERROR_CONNECTION_REFUSED) {
      return "検索サービスがタイムアウトしました。ネットワーク状態を確認してください。";
    }
    return String("検索に失敗しました（HTTP ") + code + "）。";
  }

  String body = http.getString();
  http.end();
  Serial.printf("[jina] body length=%d\n", (int)body.length());
  // 先頭 200 バイトをログ（JSON 構造の確認）
  Serial.printf("[jina] body head: %s\n",
                body.substring(0, body.length() > 200 ? 200 : body.length()).c_str());

  // no-content 指定で title/url/description のみ受け取るが、念のため Filter で
  // 必要フィールドだけ抽出する（多言語フィールド・メタ情報を読み飛ばす）。
  StaticJsonDocument<256> filter;
  filter["data"][0]["title"] = true;
  filter["data"][0]["url"] = true;
  filter["data"][0]["description"] = true;
  filter["data"][0]["content"] = true;

  SpiRamJsonDocument doc(32 * 1024);
  DeserializationError err = deserializeJson(doc, body,
                                             DeserializationOption::Filter(filter));
  if (err) {
    Serial.printf("[jina] json parse error: %s (body length=%d)\n",
                  err.c_str(), (int)body.length());
    return "検索結果の解析に失敗しました。";
  }

  JsonArray data = doc["data"].as<JsonArray>();
  if (data.isNull() || data.size() == 0) {
    Serial.println("[jina] data array empty or missing");
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
