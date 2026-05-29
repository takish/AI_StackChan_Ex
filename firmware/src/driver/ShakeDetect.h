#ifndef _SHAKE_DETECT_H
#define _SHAKE_DETECT_H

#include <Arduino.h>

// CoreS3 / Core2 内蔵 IMU の加速度センサで「本体が激しく振られた」を検出する。
// 公式 stackchan（ESP-IDF 版）の MotionDetector を参考に、加速度の差分
// （ハイパスフィルタ相当）が閾値を超えるピークを時間窓内に複数回数える方式。
//
// 頭撫で検出（HeadPetDetect）より明確に強い動きだけを拾うため、閾値・回数を
// 大きく設定している。API は HeadPetDetect と同じ消費型フラグに揃えてある。
//
// loop() から shakeDetected() を毎周回呼び出すと、検出が立っていれば true を
// 返してフラグをクリアする（消費型）。

// タスクを起動（M5.begin() より後で呼ぶこと）
void invokeShakeDetectTask();

// 検出フラグの取得（true を一度返すと自動でクリアされる）
bool shakeDetected();

// サーボ動作などで誤検出を避けたい間、検出を抑制する
void shakeMaskFor(uint32_t ms);

#endif  // _SHAKE_DETECT_H
