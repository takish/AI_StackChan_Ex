#ifndef _TOOL_BASE_H
#define _TOOL_BASE_H

#include <Arduino.h>
#include <ArduinoJson.h>

// LLM Function Calling の 1 関数を表す抽象基底。
//
// 各 Tool 派生クラスは、関数名・OpenAI/Gemini 互換の schema・実行ハンドラを
// 提供する。FunctionCall::register_tool() に登録すると、`exec_calledFunc` から
// dispatch されるようになる。
//
// 設計意図:
//   - LLM provider に依存しない（schema JSON は OpenAI/Gemini 共通）
//   - 関数の追加は新規 .cpp/.h ペアの作成 + main.cpp での register_tool() のみ
//   - 既存の巨大 if-else cascade（FunctionCall::exec_calledFunc 内）を段階的に
//     これに置き換えていく
//   - main.cpp が composition root として全 tool を持つ（CLAUDE.md レイヤー規約遵守）

class ToolBase {
public:
  virtual ~ToolBase() = default;

  // 関数名（LLM が呼び出す識別子）
  virtual const char* name() const = 0;

  // schema_json: 1 つの function 定義の JSON オブジェクト全文。
  //
  // 必ず `{ "name": "...", "description": "...", "parameters": { ... } }` の形を返す。
  // FunctionCall::combined_functions_json() が json_Functions 配列と結合する。
  //
  // 例:
  //   return R"({
  //     "name": "get_weather",
  //     "description": "指定都市の天気を取得",
  //     "parameters": {
  //       "type": "object",
  //       "properties": { "city": { "type": "string" } },
  //       "required": ["city"]
  //     }
  //   })";
  virtual const char* schema_json() const = 0;

  // 実行ハンドラ。args は LLM から渡された function arguments の JSON オブジェクト。
  // 返り値は LLM に渡すレスポンス（テキスト）。
  virtual String execute(JsonObject args) = 0;
};

#endif  // _TOOL_BASE_H
