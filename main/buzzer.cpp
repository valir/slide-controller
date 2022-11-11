#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "buzzer.h"

Buzzer buzzer;

#define BUZZER_PIN 21

void Buzzer::init()
{
  if (_pin == 0) {
    _pin = BUZZER_PIN;
    ledcAttachPin(_pin, 0);
  }
}

void Buzzer::tone(note_t note, uint8_t octave, uint16_t dur)
{
  init();
  ledcWriteNote(0, note, octave);
  vTaskDelay(pdMS_TO_TICKS(dur));
  ledcWrite(0, 0);
}

void Buzzer::doorbell()
{
  tone(NOTE_G, 6, 500);
  tone(NOTE_E, 6, 1000);
}

esp_timer_handle_t alertTimerHandle = nullptr;

void alertTimer(void* pThis) {
  reinterpret_cast<Buzzer*>(pThis)->tone(NOTE_A, 7, 1000);
}

void Buzzer::startAlert() {
  if (alertTimerHandle != nullptr)
    return; // we are already emmitting alerts
  alertTimer(reinterpret_cast<void*>(this)); // initial sound, immediately
  esp_timer_create_args_t timer_args = { .callback = alertTimer,
    .arg = reinterpret_cast<void*>(this),
    .dispatch_method = ESP_TIMER_TASK,
    .name = "alertTimer",
    .skip_unhandled_events = true };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &alertTimerHandle));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(alertTimerHandle, 2 * 1000 * 1000));
}

void Buzzer::stopAlert() {
  esp_timer_stop(alertTimerHandle);
  esp_timer_delete(alertTimerHandle);
  alertTimerHandle = nullptr;
}

void Buzzer::ackTone()
{
  tone(NOTE_A, 6, 100);
  vTaskDelay(pdMS_TO_TICKS(100));
  tone(NOTE_A, 6, 100);
}

void Buzzer::playStartup() {
  tone(NOTE_A, 6, 100);
}

void Buzzer::playOtaStart() {
  tone(NOTE_A, 6, 30);
  vTaskDelay(pdMS_TO_TICKS(30));
  tone(NOTE_A, 6, 30);
}

void Buzzer::playOtaFailed() {
  tone(NOTE_A, 4, 1000);
}

void Buzzer::playOtaDownloading() {
  tone(NOTE_A, 7, 10);
}

