#include "ShakeDetect.h"
#include <M5Unified.h>
#include <math.h>

namespace {

TaskHandle_t s_task = nullptr;
volatile bool s_detected = false;
volatile uint32_t s_mask_until_ms = 0;

// 1 サンプル間の加速度変化量がこれ以上ならスパイクとみなす [g]
// 頭撫で（0.18g）より大きくし、意図的な「振り」だけを拾う
constexpr float SPIKE_THRESHOLD = 1.2f;
// スパイクを数える時間窓
constexpr uint32_t SPIKE_WINDOW_MS = 1000;
// この回数のスパイクが時間窓内に発生したらシェイク判定
constexpr int SPIKE_COUNT = 4;
// ポーリング間隔（速い振りを拾うため頭撫での 50ms より高頻度）
constexpr int POLL_INTERVAL_MS = 25;
// 検出後のクールダウン（誤連発防止 / 演出時間より少し長め）
constexpr uint32_t COOLDOWN_MS = 5000;

void task_main(void*) {
  Serial.println("ShakeDetect task started");
  float prev_x = 0, prev_y = 0, prev_z = 0;
  bool first = true;
  uint32_t spike_times[SPIKE_COUNT] = {0};
  int spike_count = 0;

  while (true) {
    uint32_t now = millis();

    // マスク中は検出停止
    if (now < s_mask_until_ms) {
      spike_count = 0;
      first = true;
      vTaskDelay(POLL_INTERVAL_MS / portTICK_PERIOD_MS);
      continue;
    }

    float ax, ay, az;
    if (M5.Imu.getAccel(&ax, &ay, &az)) {
      if (first) {
        prev_x = ax; prev_y = ay; prev_z = az;
        first = false;
      } else {
        float dx = ax - prev_x;
        float dy = ay - prev_y;
        float dz = az - prev_z;
        float jerk = sqrtf(dx*dx + dy*dy + dz*dz);
        prev_x = ax; prev_y = ay; prev_z = az;

        // 古いスパイクを破棄
        int j = 0;
        for (int i = 0; i < spike_count; i++) {
          if (now - spike_times[i] <= SPIKE_WINDOW_MS) {
            spike_times[j++] = spike_times[i];
          }
        }
        spike_count = j;

        if (jerk > SPIKE_THRESHOLD) {
          if (spike_count < SPIKE_COUNT) {
            spike_times[spike_count++] = now;
          }
          if (spike_count >= SPIKE_COUNT) {
            Serial.printf("Shake detected (jerk=%.3f, spikes=%d in %dms)\n",
                          jerk, spike_count, (int)(now - spike_times[0]));
            s_detected = true;
            spike_count = 0;
            s_mask_until_ms = now + COOLDOWN_MS;
          }
        }
      }
    }

    vTaskDelay(POLL_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

}  // namespace

void invokeShakeDetectTask() {
  if (s_task != nullptr) return;
  xTaskCreate(task_main, "shake", 3 * 1024, nullptr, 2, &s_task);
}

bool shakeDetected() {
  if (s_detected) {
    s_detected = false;
    return true;
  }
  return false;
}

void shakeMaskFor(uint32_t ms) {
  s_mask_until_ms = millis() + ms;
}
