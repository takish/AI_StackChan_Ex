#ifndef _WEB_SEARCH_TOOL_H
#define _WEB_SEARCH_TOOL_H

#include "../ToolBase.h"

// Jina Search を介して Web を検索する汎用 Tool。
//
// 旧 get_news / get_weather は薄いラッパーで、内部では同じ jina_search を
// 呼んでいただけだった。LLM 視点では「都市/トピックを必ず指定させる」原因に
// なっており、対話が冗長だった。
//
// この Tool は generic な query を受け取って LLM 側にクエリ組み立てを任せる。
// 「今日の天気は」→ "東京 天気 今日"、「最新の AI ニュース」→ "AI 最新ニュース"
// のように LLM が文脈から自然なクエリを作る前提。
class WebSearchTool : public ToolBase {
public:
  const char* name() const override { return "search_web"; }
  const char* schema_json() const override;
  String execute(JsonObject args) override;
};

#endif  // _WEB_SEARCH_TOOL_H
