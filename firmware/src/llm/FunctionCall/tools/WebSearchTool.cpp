#include "WebSearchTool.h"
#include "JinaSearch.h"

const char* WebSearchTool::schema_json() const {
  return
    "{"
      "\"name\": \"search_web\","
      "\"description\": \"インターネットで Web 検索する。天気・ニュース・最新情報など、自分の知識にない事柄を調べる時に使う。query にはユーザーの言葉を検索エンジン向けの自然なクエリに変換して渡す。例: '今日の天気' → '東京 天気 今日'、'AIのニュース' → 'AI 最新ニュース'。都市やトピックが指定されていない場合は、会話履歴や常識から推測して補う。\","
      "\"parameters\": {"
        "\"type\": \"object\","
        "\"properties\": {"
          "\"query\": {"
            "\"type\": \"string\","
            "\"description\": \"検索エンジンに投げるクエリ文字列（日本語可）\""
          "}"
        "},"
        "\"required\": [\"query\"]"
      "}"
    "}";
}

String WebSearchTool::execute(JsonObject args) {
  const char* q_cstr = args["query"];
  String query = q_cstr ? String(q_cstr) : String("");
  if (query.length() == 0) {
    return "検索クエリが空です。";
  }
  Serial.printf("[search_web] query=%s\n", query.c_str());
  String result = jina_search(query, 3);
  return "「" + query + "」の検索結果:\n" + result;
}
