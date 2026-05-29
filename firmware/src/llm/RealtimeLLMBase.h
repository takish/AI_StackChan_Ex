#if defined(REALTIME_API)

#ifndef _REALTIME_LLM_BASE_H
#define _REALTIME_LLM_BASE_H

#include <Arduino.h>
#include <M5Unified.h>
#include "StackchanExConfig.h"
#include "SpiRamJsonDocument.h"
#include "ChatHistory.h"
#include "LLMBase.h"
#include <WebSocketsClient.h>

//#define REALTIME_API_RECORD_TEST

#define GEMINI_PROMPT_MAX_SIZE   (1024*50)

#define RT_REC_LENGTH       (2000)      //0.125s 
#define RT_REC_SAMPLE_RATE  (16000)

#ifdef REALTIME_API_RECORD_TEST
#define REALTIME_RECORD_TIMEOUT     (4 * 1000)      //ms  ※録音テスト再生用バッファのサイズに合わせる
#else
#define REALTIME_RECORD_TIMEOUT     (30 * 1000)      //ms
#endif

extern String InitBuffer;
extern const String json_ChatString;

class RealtimeLLMBase: public LLMBase{
//private:
public:   //本当はprivateにしたいところだがコールバック関数にthisポインタを渡して使うためにpublicとした
    WebSocketsClient webSocket;
    SpiRamJsonDocument msgDoc;

    // for record
    //
    //int16_t* rtRecBuf;
    int rtRecSamplerate;
    int rtRecLength;
    bool realtime_recording;
    bool response_done;
    portTickType startTime;

#ifdef REALTIME_API_RECORD_TEST
    int16_t* recTestBuf;
    int recTestLenMax;
    int recTestLenCnt;
#endif

    // for play (producer=webSocketタスク / consumer=audioPlayタスク に分離)
    //
    // Gemini は応答冒頭で音声をバースト送信するため、深いリングバッファ(PSRAM)で
    // 受信のムラを吸収し、専任 consumer タスクが一定速度で取り出して再生する。
    // これにより「最初は滑らか→数秒後にダダダ途切れる」(2枠キューの枯渇)を解消する。
    //
    // 同期は SPSC ロックフリー: producer は _ringHead のみ更新、consumer は _ringTail
    // のみ更新する(32bit整列 volatile の読み書きは ESP32 でアトミック)。
    static const uint8_t RT_SPK_CH        = 0;            // 再生専用スピーカーチャンネル
    static const size_t  RT_RING_SIZE     = 384 * 1024;   // リング容量(≒8s @24kHz/16bit)
    static const size_t  RT_PLAY_FRAME    = 8192;         // consumer の取り出し単位(≒170ms)
    static const size_t  RT_PREBUFFER     = 144 * 1024;   // 再生開始前/再開前に貯める量(≒3s)
                                                          //   大きいほど文間ポーズに強いが初回発話が遅れる(8s容量内で調整可)
    uint8_t* _audioRing;                    // リング本体(PSRAM)
    volatile size_t _ringHead;              // 書き込み位置(producer専用)
    volatile size_t _ringTail;              // 読み出し位置(consumer専用)
    uint8_t* _playFrame[3];                 // playRaw 用ローテーションバッファ(参照されるため3枚)
    int _playFrameIdx;
    uint8_t* _decodeBuf;                    // base64 デコード用スクラッチ
    volatile bool _playoutStarted;          // プリバッファ充足後 true
    volatile bool _flushPlayout;            // turnComplete 時 true(残量を全部吐き出す)
    volatile int16_t _audioLevelSample;     // リップシンク用の代表サンプル

public:
    RealtimeLLMBase(llm_param_t param);

    virtual void chat(String text, const char *base64_buf = NULL) {};   //dummy
    virtual String& buildInputAudioJson(String& jsonBuf, String& base64) = 0;

    void invokeWebSocketLoopTask(void);
    void invokeAudioPlayTask(void);
    void suspendWebSocketLoopTask(void);
    void resumeWebSocketLoopTask(void);
    void webSocketProcess();
    void audioPlayProcess(void);
    size_t audioRingAvailable(void);    // 読み出し可能バイト数
    size_t audioRingFree(void);         // 書き込み可能バイト数
    void   audioRingWrite(const uint8_t* data, size_t len);
    void   audioRingReset(void);
    int getAudioLevel();
    void startRealtimeRecord();
    void stopRealtimeRecord();
    void resetRealtimeRecordStartTime();
    portTickType checkRealtimeRecordTimeout();
    bool isRealtimeRecording() {return realtime_recording;};

    int base64_decode(const char* input, int size, char* output);
    void hexdump(const void *mem, uint32_t len, uint8_t cols = 16);
    void streamAudioDelta(String& delta);

    // for TTS
    //
    String outputText;

};


#endif  //_REALTIME_LLM_BASE_H

#endif  //REALTIME_API