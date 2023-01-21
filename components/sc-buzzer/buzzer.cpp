#include <esp_log.h>
#include <esp_pm.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "buzzer.h"
#include "backlight.h"

Buzzer buzzer;

static const char* TAG = "BUZZ";

#define BUZZER_PIN 21

class PeriodicBeep {
  private:
  uint16_t _period;
  note_t _note;
  uint8_t _octave;
  uint16_t _dur;
  bool _started = false;
  esp_timer_handle_t _alertTimerHandle = nullptr;
  bool _screenOn = false;

  public:
  PeriodicBeep(uint16_t period, note_t note, uint8_t octave, uint16_t dur, bool screenOn = false)
      : _period(period) // ms
      , _note(note)
      , _octave(octave)
      , _dur(dur) // ms
      , _screenOn(screenOn)
  {
    assert(period > dur);
  }
  void start()
  {
    if (_started)
      return; // we are already emmitting alerts
    _started = true;
    esp_timer_create_args_t timer_args = { .callback = timerFunc,
      .arg = reinterpret_cast<void*>(this),
      .dispatch_method = ESP_TIMER_TASK,
      .name = "PeriodicBeep",
      .skip_unhandled_events = true };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &_alertTimerHandle));
    emitBeep(); // initial sound, immediately
    ESP_ERROR_CHECK(
        esp_timer_start_periodic(_alertTimerHandle, _period * 1000));
    ESP_LOGI(TAG, "start with %d %d", _note, _octave);
  }
  void stop()
  {
    esp_timer_stop(_alertTimerHandle);
    esp_timer_delete(_alertTimerHandle);
    _started = false;
  }

  private:
  static void timerFunc(void* pThis)
  {
    xTaskCreatePinnedToCore(beepTask, "beepTask", 2048, pThis, 1, nullptr, 1);
  }
  static void beepTask(void* pThis)
  {
    assert(pThis != nullptr);
    reinterpret_cast<PeriodicBeep*>(pThis)->emitBeep();
    vTaskDelete(nullptr);
  }
  void emitBeep() {
    if (_screenOn)
      BackLight::turnOn();
    buzzer.tone(_note, _octave, _dur); }
};

SemaphoreHandle_t buzzerMutex = nullptr;
esp_pm_lock_handle_t pmLock = nullptr;

void Buzzer::init()
{
  if (_pin == 0) {
    _pin = BUZZER_PIN;
    ledcAttachPin(_pin, 0);
  }
  if (nullptr == buzzerMutex) {
    buzzerMutex = xSemaphoreCreateMutex();
  }
  if (nullptr == pmLock) {
    ESP_ERROR_CHECK(esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, NULL, &pmLock));
  }
}

void Buzzer::tone(note_t note, uint8_t octave, uint16_t dur)
{
  init();
  if (xSemaphoreTake(buzzerMutex, portMAX_DELAY) == pdTRUE) {
    ESP_ERROR_CHECK(esp_pm_lock_acquire(pmLock));
    if (0 == ledcWriteNote(0, note, octave)) {
      ESP_LOGE(TAG, "Cannot write note %d, octave %d", note, octave);
    }
    vTaskDelay(pdMS_TO_TICKS(dur));
    ledcWrite(0, 0);
    esp_pm_lock_release(pmLock);
    xSemaphoreGive(buzzerMutex);
  }
}

void Buzzer::doorbell()
{
  tone(NOTE_G, 6, 500);
  tone(NOTE_E, 6, 1000);
}

PeriodicBeep alertBeep(2 * 1000, NOTE_A, 7, 1000);

void Buzzer::startAlert() { alertBeep.start(); }

void Buzzer::stopAlert() { alertBeep.stop(); }

PeriodicBeep warningBeep(2 * 1000, NOTE_D, 5, 1000, true);

void Buzzer::startWarning() { warningBeep.start(); }

void Buzzer::stopWarning() { warningBeep.stop(); }

void Buzzer::ackTone()
{
  tone(NOTE_A, 6, 100);
  vTaskDelay(pdMS_TO_TICKS(100));
  tone(NOTE_A, 6, 100);
}

void Buzzer::playStartup() { tone(NOTE_A, 6, 100); }

void Buzzer::playOtaStart()
{
  tone(NOTE_A, 6, 30);
  vTaskDelay(pdMS_TO_TICKS(30));
  tone(NOTE_A, 6, 30);
}

void Buzzer::playOtaFailed() { tone(NOTE_A, 4, 1000); }

void Buzzer::playOtaDownloading() { tone(NOTE_A, 7, 10); }

void Buzzer::swipeDetectedTone() { tone(NOTE_C, 8, 40); }
