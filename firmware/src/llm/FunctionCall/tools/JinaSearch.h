#ifndef _JINA_SEARCH_H
#define _JINA_SEARCH_H

#include <Arduino.h>

// Jina Search API (https://s.jina.ai/?q=...) で検索し、上位 max_results 件を
// 1〜N 番付きテキストで返す。API キー不要。
//
// 失敗時はエラーメッセージ文字列を返す（LLM へそのまま流せる体裁）。
String jina_search(const String& query, int max_results = 3);

#endif  // _JINA_SEARCH_H
