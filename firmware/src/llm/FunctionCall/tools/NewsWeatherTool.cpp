#include "NewsWeatherTool.h"
#include "JinaSearch.h"

const char* GetNewsTool::schema_json() const {
  return
    "{"
      "\"name\": \"get_news\","
      "\"description\": \"指定したトピックに関する最新ニュースをインターネットから検索して取得する。例: 'AI', 'スポーツ', 'M5Stack'\","
      "\"parameters\": {"
        "\"type\":\"object\","
        "\"properties\": {"
          "\"topic\": {"
            "\"type\": \"string\","
            "\"description\": \"検索するニュースのトピック（日本語可）\""
          "}"
        "},"
        "\"required\": [\"topic\"]"
      "}"
    "}";
}

String GetNewsTool::execute(JsonObject args) {
  const char* topic_cstr = args["topic"];
  String topic = topic_cstr ? String(topic_cstr) : String("");
  Serial.printf("[get_news] topic=%s\n", topic.c_str());
  String result = jina_search(topic + " 最新ニュース", 3);
  return "「" + topic + "」のニュース:\n" + result;
}


const char* GetWeatherTool::schema_json() const {
  return
    "{"
      "\"name\": \"get_weather\","
      "\"description\": \"指定した都市の今日の天気をインターネットから取得する。例: '東京', '大阪', '札幌'\","
      "\"parameters\": {"
        "\"type\":\"object\","
        "\"properties\": {"
          "\"city\": {"
            "\"type\": \"string\","
            "\"description\": \"天気を知りたい都市名（日本語可）\""
          "}"
        "},"
        "\"required\": [\"city\"]"
      "}"
    "}";
}

String GetWeatherTool::execute(JsonObject args) {
  const char* city_cstr = args["city"];
  String city = city_cstr ? String(city_cstr) : String("東京");
  Serial.printf("[get_weather] city=%s\n", city.c_str());
  String result = jina_search(city + " 天気 今日", 2);
  return city + " の天気:\n" + result;
}
