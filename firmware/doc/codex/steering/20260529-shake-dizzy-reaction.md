# シェイク → 酔う（ぐるぐる目）リアクション

作成日: 2026-05-29

## 背景・目的

M5Stack 公式の新ファーム（`../stackchan`, ESP-IDF + LVGL ベース）には
「本体を振ると目がぐるぐる回って酔った顔になる」リアクションが実装されている。

- 検知: `firmware/main/hal/utils/motion_detector/motion_detector.h` の `MotionDetector`
  （加速度の差分 = ハイパスフィルタ → 100ms デバウンス → 1秒窓で 3 回ピークでシェイク確定）
- 発火: `hal_imu.cpp` が `setShakeThreshold(16.0f)` で閾値設定し `onImuMotionEvent.emit(Shake)`
- 演出: `stackchan/modifiers/imu.h` の `ImuEventModifier` が 4 秒間、
  両目を消して `DizzyDecorator`（渦巻き画像を 300ms 毎に 30 度回転）＋ `ShyDecorator`（ほっぺ）
  ＋ 口を 600ms 毎に傾けてパクパク ＋ サーボを home に固定。

これを本リポジトリ（Arduino + m5stack-avatar）に移植する。

## アーキテクチャ差分と方針

| | 公式 `../stackchan` | 本リポジトリ |
|---|---|---|
| 基盤 | ESP-IDF + LVGL + uitk | Arduino + m5stack-avatar |
| ぐるぐる目 | 渦巻き画像を回転（Decorator） | **標準に無い → `setGaze()` を円運動させ目玉を回して近似** |

検知ロジック（差分→デバウンス→時間窓カウント）は概念ごと流用できる。
本リポジトリには既に酷似した `driver/HeadPetDetect.cpp` があるため、それをお手本にする。

## レイヤー割り当て（CLAUDE.md 準拠）

- **L2 `driver/ShakeDetect.{h,cpp}`（新規）**: IMU から激しいシェイクを検知する純粋ドライバ。
  Avatar / Robot に依存しない。`invokeShakeDetectTask()` / `shakeDetected()`（消費型）/
  `shakeMaskFor(ms)` を公開。`HeadPetDetect` と同じ消費型フラグ API に揃える。
- **L6 `main.cpp`（composition root / presentation）**: `dizzy_trigger()` / `dizzy_update()` を
  `head_pet_trigger` / `head_pet_update` と同じパターンで実装。具体的な avatar / robot / idle_motion
  の操作はここに閉じる（driver から上位を触らない原則を維持）。

## 検知パラメータ（頭撫でとの差別化）

頭撫で（`HeadPetDetect`）は jerk > 0.18g が 900ms 窓内に 3 回。
シェイクはこれより明確に強い動きだけ拾う:

- `SHAKE_SPIKE_THRESHOLD = 1.2f` [g]（フレーム間加速度変化）
- `SHAKE_WINDOW_MS = 1000`
- `SHAKE_COUNT = 4`
- `POLL_INTERVAL_MS = 25`（速い振りを拾うため頭撫での 50ms より高頻度）
- `COOLDOWN_MS = 5000`

激しいシェイクは頭撫で検出器も同時に発火させてしまうため、loop() では
`shakeDetected()` を先に判定し、発火時は `headPetMaskFor()` で頭撫でを抑制 ＋
保留中の頭撫でフラグを `headPetDetected()` 呼び出しで破棄する。

## 演出（dizzy_trigger / dizzy_update）

- 表情: `Expression::Sleepy`（とろん）
- 目玉ぐるぐる: `setGaze(sinf(θ), cosf(θ))` を毎フレーム円運動（周期 ~600ms）
- 口: `setMouthOpenRatio()` を θ で揺らす（気持ち悪そう）
- 頭の揺れ: `setRotation()` を小さく sin 揺らし
- セリフ: `setSpeechText("ぐるぐる…")`
- LED: 緑系（気分が悪い色）
- サーボ: `idle_motion_pause()` ＋ home へ。600ms 毎に小さく左右へ sway
- 継続時間: `DIZZY_DURATION_MS = 4000`。終了で全状態を Neutral に復帰し
  `idle_motion_resume()`。

## 対象ボード

`HeadPetDetect` と同じく IMU 搭載機（Core2 / CoreS3）。loop() 内の
`#if defined(ARDUINO_M5STACK_Core2) || defined(ARDUINO_M5STACK_CORES3)` ガード内に配置。
IMU 非搭載機では `M5.Imu.getAccel()` が false を返すため実害なし。

## 未対応 / 任意拡張

- 効果音（「うぇ〜」mp3）は音源ファイルを同梱しないため今回はスコープ外。
  必要なら `playMP3SD("/dizzy.mp3")` を `dizzy_trigger()` に追加する。
- 本物の渦巻き目が欲しい場合は avatar の Canvas へ自前描画が必要（別 PR）。
