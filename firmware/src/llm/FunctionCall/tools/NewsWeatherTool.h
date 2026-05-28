#ifndef _NEWS_WEATHER_TOOL_H
#define _NEWS_WEATHER_TOOL_H

#include "../ToolBase.h"

// Jina Search 経由で最新ニュースを取得する Function Tool
class GetNewsTool : public ToolBase {
public:
  const char* name() const override { return "get_news"; }
  const char* schema_json() const override;
  String execute(JsonObject args) override;
};

// Jina Search 経由で指定都市の今日の天気を取得する Function Tool
class GetWeatherTool : public ToolBase {
public:
  const char* name() const override { return "get_weather"; }
  const char* schema_json() const override;
  String execute(JsonObject args) override;
};

#endif  // _NEWS_WEATHER_TOOL_H
