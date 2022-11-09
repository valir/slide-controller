

#include "backlight.h"
#include "display.h"
#include "events.h"
#include <XPT2046_Touchscreen.h>
#include <driver/timer.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl/lvgl.h>

#define TOUCH_CS 14
#define TOUCH_IRQ 27

#define HAVE_TOUCHPAD

#define TS_MINX 370
#define TS_MINY 470
#define TS_MAXX 3700
#define TS_MAXY 3600

static const char* TAG = "TOUCH";

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
TS_Point oldPoint;
extern TaskHandle_t touchScreenTaskHandle;

enum TouchGesture {
  TOUCH_OFF         = 0b00000000000000000000000000000001,
  TOUCH_ON          = 0b00000000000000000000000000000010,
  TOUCH_LONG_PRESS  = 0b00000000000000000000000000000100,
  TOUCH_SWIPE_LEFT  = 0b00000000000000000000000000001000,
  TOUCH_SWIPE_RIGHT = 0b00000000000000000000000000010000,
  TOUCH_SWIPE_UP    = 0b00000000000000000000000000100000,
  TOUCH_SWIPE_DOWN  = 0b00000000000000000000000001000000
};

bool touched = false;
esp_timer_handle_t touchTimerHandle;
bool timerInitialized = false;

void touchTimer(void*)
{
  touched = false;
  ESP_LOGD(TAG, "Finger released");
  xTaskNotify(touchScreenTaskHandle, TOUCH_OFF, eSetBits);
}

void resetTouchTimer()
{
  if (!timerInitialized) {
    esp_timer_create_args_t timer_args = { .callback = touchTimer,
      .arg = NULL,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "touchTimer",
      .skip_unhandled_events = true };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &touchTimerHandle));
    timerInitialized = true;
  }
  esp_timer_stop(touchTimerHandle);
  ESP_ERROR_CHECK(esp_timer_start_once(touchTimerHandle, 300 * 1000));
}

void touch_screen_input(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
  if (ts.tirqTouched()) {
    if (ts.touched()) {
      vTaskDelay(pdMS_TO_TICKS(30)); // debounce the touch gesture
      if (ts.touched()) {
        resetTouchTimer();
        if (!touched) {
          touched = true; // avoid sending multiple touched events
          auto p = ts.getPoint();
          p.x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
          p.y = map(p.y, TS_MINY, TS_MAXY, 0, 240);
          ESP_LOGD(TAG, "Touched at %d,%d with pressure %d", p.x, p.y, p.z);
          oldPoint = p;
          data->state = LV_INDEV_STATE_PR;
          data->point.x = p.x;
          data->point.y = p.y;

          xTaskNotify(touchScreenTaskHandle, TOUCH_ON, eSetBits);
          return;
        } else {
          // TODO detect gestures here
        }
      }
    }
  }

  data->state = LV_INDEV_STATE_REL;
  data->point.x = oldPoint.x;
  data->point.y = oldPoint.y;
}

extern bool night_mode;
void setDisplayBacklight(bool);

void touchScreenTask(void*)
{
  if (!ts.begin(displayTaskHandle, DISPLAY_NOTIFY_TOUCH)) {
    ESP_LOGE(TAG, "Cannot setup touchscreen");
    vTaskDelete(NULL);
    return;
  }
  ts.setRotation(3);

  for (;;) {
    // clear bits on entry to avoid multiple events at once when screen is
    // touched
    uint32_t notif_flags = 0;
    xTaskNotifyWait(ULONG_MAX, ULONG_MAX, &notif_flags, portMAX_DELAY);
    if (notif_flags & TOUCH_ON) {
      if (!BackLight::isOn()) {
        BackLight::turnOn();
        vTaskDelay(pdMS_TO_TICKS(
            1000)); // prevent unintentional touched event trigger
      } else {
        lv_point_t touchedPoint = { .x = oldPoint.x, .y = oldPoint.y };
        if (lv_scr_act()
            == lv_indev_search_obj(lv_scr_act(), &touchedPoint)) {
          // user touched the screen, and did not hit any object, so let's
          // send the touched event to home assistant
          events.postTouchedEvent();
          vTaskDelay(pdMS_TO_TICKS(1000)); // wait a bit to avoid repeat
                                           // sending for same touch event
          xTaskNotifyStateClear(NULL);
        }
      }
    }
  }
}
