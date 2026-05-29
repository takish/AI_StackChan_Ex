#if defined(REALTIME_API)

#include <Arduino.h>
#include <M5Unified.h>
#include <Avatar.h>
#include "share/Mutex.h"
//#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "rootCA/rootCAgoogleGemini.h"
#include <ArduinoJson.h>
#include "SpiRamJsonDocument.h"
#include "RealtimeLLMBase.h"
//#include "FunctionCall.h"
//#include "MCPClient.h"
#include "Robot.h"

#include <base64.h>
#include "libb64/cdecode.h"
#include <WebSocketsClient.h>

using namespace m5avatar;
extern Avatar avatar;

int16_t rtRecBuf[RT_REC_LENGTH];    // リアルタイム録音用メモリ
                                    // Core2だとヒープが不足するので静的な配列とした

TaskHandle_t webSocketLoopTask_h = NULL;
TaskHandle_t audioPlayTask_h = NULL;

// WebSocketのイベント処理(webSocket.loop())及び、録音データ（約0.1秒）を
// WebSocketで送信するためのループタスク
void webSocketLoopTask(void *arg) {
    Serial.println("WebSocket loop task created");
    RealtimeLLMBase* pThis = (RealtimeLLMBase*)arg;

    while(1){
        pThis->webSocketProcess();
        //delay(1);     //webSocketProcess()内で状態によってスリープ時間を変更
    }
}

#ifndef REALTIME_API_WITH_TTS
// 再生専任タスク(consumer)。リングバッファから音声を一定速度で取り出して再生する。
// webSocketLoopTask(producer)と分離することで、受信のムラが再生に波及しない。
void audioPlayTask(void *arg) {
    Serial.println("Audio play task created");
    RealtimeLLMBase* pThis = (RealtimeLLMBase*)arg;

    while(1){
        pThis->audioPlayProcess();
    }
}
#endif


RealtimeLLMBase::RealtimeLLMBase(llm_param_t param) : 
    LLMBase(param, 0),
    msgDoc(0),
    rtRecSamplerate(RT_REC_SAMPLE_RATE),
    rtRecLength(RT_REC_LENGTH),
    realtime_recording(false),
    response_done(false),
    startTime(0),
    _audioRing(nullptr),
    _ringHead(0),
    _ringTail(0),
    _playFrameIdx(0),
    _decodeBuf(nullptr),
    _playoutStarted(false),
    _flushPlayout(false),
    _audioLevelSample(0),
    outputText(String(""))
{
#ifdef REALTIME_API_RECORD_TEST
  // リアルタイム録音のチャンクデータを蓄積してテスト再生するためのバッファ（約4s）
  recTestLenMax = rtRecLength * 40;
  recTestLenCnt = 0;
  recTestBuf = (int16_t*)heap_caps_malloc(recTestLenMax * sizeof(*rtRecBuf), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif

#ifndef REALTIME_API_WITH_TTS
  // 深いリングバッファ(PSRAM)・デコード用スクラッチ(PSRAM)・playRaw 用フレーム(内部RAM×3)
  _audioRing = (uint8_t*)heap_caps_malloc(RT_RING_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  _decodeBuf = (uint8_t*)heap_caps_malloc(100 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  for(int i=0; i<3; i++){
    _playFrame[i] = (uint8_t*)heap_caps_malloc(RT_PLAY_FRAME, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
#endif

}

void RealtimeLLMBase::webSocketProcess()
{
    webSocket.loop();

#ifdef REALTIME_API_WITH_TTS
    if(response_done && !speaking){
        startRealtimeRecord();
        response_done = false;
    }
#endif

    if(realtime_recording){
        enterMutexAudio();
        //M5.Mic.begin();
        if(!M5.Mic.record(rtRecBuf, rtRecLength, rtRecSamplerate)){
            Serial.println("Mic.record() returns false");
            delay(1000);
        }
        //M5.Mic.end();
        exitMutexAudio();
        String audio_base64;
        audio_base64 = base64::encode((u8*)rtRecBuf, rtRecLength * sizeof(int16_t));

#ifdef REALTIME_API_RECORD_TEST
        if((recTestLenCnt + rtRecLength) < recTestLenMax){
            memcpy((u8*)&recTestBuf[recTestLenCnt], (u8*)rtRecBuf, rtRecLength * sizeof(int16_t));
            recTestLenCnt += rtRecLength;
        }
#else
        String audioJsonBuf("");
        webSocket.sendTXT(buildInputAudioJson(audioJsonBuf, audio_base64));
#endif

        portTickType elapsedTime = checkRealtimeRecordTimeout();

#if 0   //Debug リスニング経過時間の表示
        static char speechTxt[64];
        sprintf(speechTxt, "Listening:%ds", int(elapsedTime / 1000));
        avatar.setSpeechText(speechTxt);
#else
        avatar.setSpeechText("Listening...");
#endif
        delay(1);
    }
    else{
        if(speaking){
            //発話中もしくはテキスト生成中
            avatar.setSpeechText("");
            resetRealtimeRecordStartTime(); //長いテキストを発話中にタイムアウトしてしまうのを防ぐ
            delay(1);
        }
        else{
            avatar.setSpeechText("Please touch");
            delay(10);
        }
    }
}

int RealtimeLLMBase::getAudioLevel()
{
    return abs(_audioLevelSample) * 50;
}

void RealtimeLLMBase::startRealtimeRecord()
{
    if(!realtime_recording){
        Serial.println("Start realtime recording");
        realtime_recording = true;
        startTime = xTaskGetTickCount();
    }
}

void RealtimeLLMBase::stopRealtimeRecord()
{
    if(realtime_recording){
        Serial.println("Stop realtime recording");
        realtime_recording = false;
        startTime = 0;
    }
}

void RealtimeLLMBase::resetRealtimeRecordStartTime()
{
    startTime = xTaskGetTickCount();
}

portTickType RealtimeLLMBase::checkRealtimeRecordTimeout()
{
    portTickType elapsedTime;
    elapsedTime = (xTaskGetTickCount() - startTime) * portTICK_RATE_MS;
    if(elapsedTime > REALTIME_RECORD_TIMEOUT){
        Serial.println("Realtime recording timeout");
        stopRealtimeRecord();
#ifdef REALTIME_API_RECORD_TEST
        M5.Mic.end();
        if (M5.Speaker.begin())
        {
            M5.Speaker.playRaw(recTestBuf, recTestLenCnt, rtRecSamplerate);
            while (M5.Speaker.isPlaying()) { delay(10); }
            M5.Speaker.end();
            M5.Mic.begin();
        }
        recTestLenCnt = 0;
#endif
    }

    return elapsedTime;
}

int RealtimeLLMBase::base64_decode(const char* input, int size, char* output)
{
	/* keep track of our decoded position */
	char* c = output;
	/* store the number of bytes decoded by a single call */
	int cnt = 0;
	/* we need a decoder state */
	base64_decodestate s;
	
	/*---------- START DECODING ----------*/
	/* initialise the decoder state */
	base64_init_decodestate(&s);
	/* decode the input data */
	cnt = base64_decode_block(input, strlen(input), c, &s);
	c += cnt;
	/* note: there is no base64_decode_blockend! */
	/*---------- STOP DECODING  ----------*/
	
	/* we want to print the decoded data, so null-terminate it: */
	*c = 0;
	
	return cnt;
}


void RealtimeLLMBase::hexdump(const void *mem, uint32_t len, uint8_t cols) {
	const uint8_t* src = (const uint8_t*) mem;
	Serial.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
			Serial.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		Serial.printf("%02X ", *src);
		src++;
	}
	Serial.printf("\n");
}


// ---- リングバッファ (SPSC: producer=webSocket, consumer=audioPlay) ----

size_t RealtimeLLMBase::audioRingAvailable(void)
{
    // 読み出し可能バイト数
    return (_ringHead - _ringTail + RT_RING_SIZE) % RT_RING_SIZE;
}

size_t RealtimeLLMBase::audioRingFree(void)
{
    // 書き込み可能バイト数 (満杯と空を区別するため1バイト空ける)
    return RT_RING_SIZE - 1 - audioRingAvailable();
}

void RealtimeLLMBase::audioRingReset(void)
{
    _ringTail = _ringHead;
    _playoutStarted = false;
}

// producer。デコード済み PCM をリングに書く。リング満杯時のみ短く待つ
// (= 既に最大量バッファ済み。consumer が一定速度で吐き出すので receive は実時間で追いつく)。
void RealtimeLLMBase::audioRingWrite(const uint8_t* data, size_t len)
{
    size_t written = 0;
    while(written < len){
        size_t freeBytes = audioRingFree();
        if(freeBytes == 0){ vTaskDelay(1); continue; }
        size_t n = len - written;
        if(n > freeBytes) n = freeBytes;
        size_t firstPart = RT_RING_SIZE - _ringHead;       // 末尾までの連続領域
        if(firstPart > n) firstPart = n;
        memcpy(_audioRing + _ringHead, data + written, firstPart);
        if(n > firstPart) memcpy(_audioRing, data + written + firstPart, n - firstPart);
        _ringHead = (_ringHead + n) % RT_RING_SIZE;        // producer のみが更新
        written += n;
    }
}

// producer 側エントリ。base64 デコード → リングへ。再生待ちはしない(consumer が担当)。
void RealtimeLLMBase::streamAudioDelta(String& delta)
{
    int len = base64_decode(delta.c_str(), delta.length(), (char*)_decodeBuf);
    if(len > 0){
        audioRingWrite(_decodeBuf, (size_t)len);
    }
}

// consumer。プリバッファ充足後、リングから一定速度で取り出して再生する。
void RealtimeLLMBase::audioPlayProcess(void)
{
    if(!speaking){
        vTaskDelay(5);
        return;
    }

    size_t avail = audioRingAvailable();

    // プリバッファ: 一定量(またはturn終了フラッシュ)まで貯めてから再生開始。
    // 冒頭のバーストを溜め込み、後半のジッタを吸収する。
    if(!_playoutStarted){
        if(avail < RT_PREBUFFER && !_flushPlayout){
            vTaskDelay(2);
            return;
        }
        _playoutStarted = true;
    }

    if(avail == 0){
        // 応答の途中で在庫切れ(配信ジッタ)。極小フラグメントを鳴らすと「ダダダ」と
        // 途切れるため、再生を一旦止めてプリバッファし直す(クリーンなポーズ→再開)。
        if(!_flushPlayout){
            _playoutStarted = false;
            Serial.println("[realtime] audio underrun, rebuffering");
        }
        vTaskDelay(1);
        return;
    }

    size_t n = (avail > RT_PLAY_FRAME) ? RT_PLAY_FRAME : avail;
    n &= ~((size_t)1);   // int16 境界に揃える
    if(n == 0){ vTaskDelay(1); return; }

    // リングからフレームバッファへコピー(playRaw はポインタ参照のため面を回す)
    uint8_t* fb = _playFrame[_playFrameIdx];
    size_t firstPart = RT_RING_SIZE - _ringTail;
    if(firstPart > n) firstPart = n;
    memcpy(fb, _audioRing + _ringTail, firstPart);
    if(n > firstPart) memcpy(fb + firstPart, _audioRing, n - firstPart);
    _ringTail = (_ringTail + n) % RT_RING_SIZE;            // consumer のみが更新

    // 専用チャンネルのキュー(2枠)が満杯の間だけ待つ。深いリングが上流にあるので
    // ここで待っても枯渇しない。連続投入で wav はギャップなく連結再生される。
    while(M5.Speaker.isPlaying(RT_SPK_CH) >= 2){ vTaskDelay(1); }
    _audioLevelSample = ((int16_t*)fb)[0];
    M5.Speaker.playRaw((int16_t*)fb, n/2, 24000, false, 1, RT_SPK_CH, false);
    _playFrameIdx = (_playFrameIdx + 1) % 3;
}

void RealtimeLLMBase::invokeWebSocketLoopTask(void)
{
    // producer(受信)は CPU0 に固定し、WiFi/lwip と同居させて RX 効率を上げる。
    xTaskCreatePinnedToCore(webSocketLoopTask, "webSocketLoopTask",
            6*1024, this, 3, &webSocketLoopTask_h, 0 /* core 0 */);

#ifndef REALTIME_API_WITH_TTS
    invokeAudioPlayTask();
#endif
}

#ifndef REALTIME_API_WITH_TTS
void RealtimeLLMBase::invokeAudioPlayTask(void)
{
    // consumer(再生)は CPU1 に固定し、CPU0 の WiFi 受信バーストから完全隔離する。
    // これによりリングに在庫がある限り一定速度で取り出せ、受信ムラが音に波及しない。
    xTaskCreatePinnedToCore(audioPlayTask, "audioPlayTask",
            4*1024, this, 3, &audioPlayTask_h, 1 /* core 1 */);
}
#endif

void RealtimeLLMBase::suspendWebSocketLoopTask(void)
{
    if (eTaskGetState(webSocketLoopTask_h) != eSuspended) {
      Serial.println("webSocketLoopTask Suspend");
      vTaskSuspend(webSocketLoopTask_h);
    }
}

void RealtimeLLMBase::resumeWebSocketLoopTask(void)
{
    if (eTaskGetState(webSocketLoopTask_h) == eSuspended) {
      Serial.println("webSocketLoopTask Resume");
      vTaskResume(webSocketLoopTask_h);
    }
}

#endif  //REALTIME_API