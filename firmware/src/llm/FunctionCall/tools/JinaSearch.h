#ifndef _JINA_SEARCH_H
#define _JINA_SEARCH_H

#include <Arduino.h>

// Jina Search API (https://s.jina.ai/?q=...) で検索し、上位 max_results 件を
// 1〜N 番付きテキストで返す。
//
// 2024年後半より s.jina.ai は API キー必須に変更された。composition root
// (main.cpp) から jina_set_api_key() で起動時に注入する。未設定の場合は
// 401 を受けてその旨のエラーメッセージを返す。
//
// 失敗時はエラーメッセージ文字列を返す（LLM へそのまま流せる体裁）。
void   jina_set_api_key(const String& key);
String jina_search(const String& query, int max_results = 3);

#endif  // _JINA_SEARCH_H
