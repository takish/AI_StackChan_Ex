# Realtime 音声再生のリングバッファ化（途切れ対策）

## 背景・症状

Gemini Live（Realtime API, AUDIO モード）で応答音声が途切れ途切れになる。
VOICEVOX(ずんだもん)は AUDIO モードでは未使用のため、TTS との競合ではない。

## 原因

`RealtimeLLMBase::streamAudioDelta()` が `webSocketLoopTask` 上で同期的に呼ばれ、

```cpp
while (M5.Speaker.isPlaying()) { vTaskDelay(1); }   // 再生完了まで待つ
M5.Speaker.playRaw(...);
```

と「前チャンクの再生完了を待ってから次を再生」していた。`webSocketProcess` は
受信(`webSocket.loop()`)・再生・録音送信を**単一タスクで直列処理**しているため、
再生待ちの間はネットワーク受信が止まり、受信⇄再生が相互にブロックして音が途切れる。

I2S のマイク↔スピーカー共有による切替グリッチは「応答の開始/終了の1回ずつ」で
しか起きず（再生中はスピーカーモード固定）、再生中の途切れの主因ではない。

## 方針：deep ring + 専任 consumer タスク + プリバッファ + 再バッファリング

最小修正（非ブロック化のみ）も試したが、実機では「最初は滑らか→数秒後にダダダ」
「文の塊ごと/単語の途中で止まる」が残った。Gemini は応答冒頭で音声をバースト送信し、
その後はほぼリアルタイム速度＋ジッタで届くため、浅い先読み（M5.Speaker の 2 枠キュー）
では枯渇する。実測（リング残量ログ）でも、配信が速い時はリングが満タンまで埋まり滑らか、
ギャップが出る時は枯渇 → 途切れ、と確認できた。よって深いバッファと供給/再生の分離が必要。

### 構成（producer / consumer 分離）

```
[webSocketLoopTask = producer, CPU0]
  受信 → base64 デコード → リングへ書くだけ（ブロックしない）
        ↓ リングバッファ 384KB（≒8s @24kHz/16bit, PSRAM）
[audioPlayTask = consumer, CPU1]
  プリバッファ(3s)充足後、固定フレーム(8KB)を取り出して playRaw
```

- **SPSC ロックフリー**：producer は `_ringHead` のみ、consumer は `_ringTail` のみ更新
  （32bit 整列 volatile の読み書きは ESP32 でアトミック）。
- **コア分離**：producer を CPU0（WiFi/lwip と同居で RX 効率）、consumer を CPU1
  （WiFi 受信バーストから隔離）に固定 → 受信ムラが再生に波及しない。
- **`disableCore0WDT()`**（main.cpp）：Realtime 中は CPU0 が音声バースト受信で飽和し
  CPU0 IDLE が動けず Task Watchdog がリブートを起こすため、CPU0 IDLE 監視を解除する。
- **プリバッファ 3s**（`RT_PREBUFFER`）：再生開始/再開前に貯めることで、Gemini の
  文間生成ポーズや配信ジッタを乗り越える。大きいほど途切れに強いが初回発話が遅れる
  （ring 容量 8s 内で調整可。1s では単語途中の途切れが残り、3s で解消を確認）。
- **再バッファリング**：応答途中で枯渇したら極小フラグメントを鳴らさず `_playoutStarted`
  を倒して貯め直す（プチプチ→クリーンなポーズ＋再開に変換）。
- **playRaw はポインタ参照のみ**（`Speaker_Class.cpp:870` `info.data = data;`、コピーしない）。
  チャンネルは wav 2 枠キュー可のため、再生中の面を上書きしないよう **3 枚の
  フレームバッファをローテーション**。`stop_current_sound=false` の連続投入で連結再生。

### ライフサイクル / 同期

- ミューテックス(`mutexAudio`)は非再帰で応答中は producer が保持。consumer は playRaw
  のみで触らない（デッドロック回避）。
- 発話開始(`speaking=true`)で `audioRingReset()` + `_flushPlayout=false`。
- `turnComplete` / Disconnect では `_flushPlayout=true` にして「リング空 かつ 再生完了」
  まで待ってから `Speaker.end()`→`Mic.begin()`（末尾切れ防止）→ `audioRingReset()`。

## 変更ファイル

- `llm/RealtimeLLMBase.h` — リング/フレーム/プリバッファ/フラグのメンバ・メソッド宣言
- `llm/RealtimeLLMBase.cpp` — alloc、`audioPlayTask`(consumer)、ring 入出力、
  `streamAudioDelta`(producer) 改修、コア固定、`getAudioLevel` をサンプル参照に
- `llm/Gemini/GeminiLive.cpp`・`llm/ChatGPT/RealtimeChatGPT.cpp` — 発話開始/終了で
  `audioRingReset()`・`_flushPlayout` 制御、ドレイン待ちに変更
- `main.cpp` — `disableCore0WDT()`

## チューニングの勘所

- 途切れる → `RT_PREBUFFER` を上げる（初回発話の遅延と引き換え）。
- 初回発話が遅い → `RT_PREBUFFER` を下げる。
- 「話す速度を落とす」は途切れ対策にはならない（音声は速度に依らず 24kHz=48KB/s 固定）。
  聞き取りやすさ目的ならプロンプト/`systemRole` で指示する。

## スコープ外（別対応）

- I2S マイク/スピーカー共有による境界グリッチ（半二重切替）の解消
- OpenAI Realtime 経路は同じ基底(`RealtimeLLMBase`)を通るため恩恵を受けるが、実機確認は Gemini のみ
