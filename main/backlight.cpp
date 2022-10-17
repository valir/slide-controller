
#include <driver/timer.h>
#include <esp_timer.h>
#include <Arduino.h>

#include "backlight.h"

esp_timer_handle_t timer = NULL;

static void turn_off_screen_cb(void* pObj)
{
  reinterpret_cast<BackLight*>(pObj)->turnOff();
}

BackLight BackLight::_instance;

BackLight::BackLight()
{
}

bool BackLight::isOn() {
  return _instance._on;
}
void BackLight::setPin(int ledPin) { _instance._ledPin = ledPin; }
void BackLight::activateTimer()
{
  esp_timer_create_args_t timer_args = { .callback = turn_off_screen_cb,
    .arg = reinterpret_cast<void*>(&_instance),
    .dispatch_method = ESP_TIMER_TASK,
    .name = "turn_off_scr",
    .skip_unhandled_events = true };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
  _instance._timerActive = true;
  triggerTimer();
}
void BackLight::triggerTimer()
{
  esp_timer_stop(
      timer); // stop timer if already running, no error check on purpose
  ESP_ERROR_CHECK(esp_timer_start_once(timer, 5000 * 1000));
}
void BackLight::turnOn()
{
  assert(_instance._ledPin != -1);
  pinMode(_instance._ledPin, OUTPUT);
  _instance._on = true;
  digitalWrite(_instance._ledPin, LOW);
  if (_instance._timerActive) {
    triggerTimer();
  }
}

void BackLight::turnOff()
{
  assert(_instance._ledPin != -1);
  pinMode(_instance._ledPin, OUTPUT);
  digitalWrite(_instance._ledPin, HIGH);
  _instance._on = false;
}
