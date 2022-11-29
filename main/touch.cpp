

#include "backlight.h"
#include "display.h"
#include "events.h"
#include <XPT2046_Touchscreen.h>
#include <algorithm>
#include <driver/timer.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl/lvgl.h>
#include <vector>

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
  TOUCH_OFF = 0b00000000000000000000000000000001,
  TOUCH_ON = 0b00000000000000000000000000000010,
  TOUCH_LONG_PRESS = 0b00000000000000000000000000000100,
  TOUCH_SWIPE_LEFT = 0b00000000000000000000000000001000,
  TOUCH_SWIPE_RIGHT = 0b00000000000000000000000000010000,
  TOUCH_SWIPE_UP = 0b00000000000000000000000000100000,
  TOUCH_SWIPE_DOWN = 0b00000000000000000000000001000000
};

bool touched = false;
esp_timer_handle_t touchTimerHandle;
bool timerInitialized = false;
std::vector<TS_Point> touchedPoints;

struct GestureDetector {
  void operator()(const TS_Point& p)
  {
    if (_at_start) {
      _at_start = false;
    } else {
      auto dx = p.x - _prev.x;
      auto dy = p.y - _prev.y;
      dx = std::abs( dx ) > THRESHOLD_X ? dx : 0;
      dy = std::abs( dy ) > THRESHOLD_Y ? dy : 0;
      if (dx && dy) {
        _maybe_gestures.push_back(TOUCH_OFF);
      } else {
        if (dx) {
          _maybe_gestures.push_back(dx > 0 ? TOUCH_SWIPE_RIGHT : TOUCH_SWIPE_LEFT);
        } else if (dy) {
          _maybe_gestures.push_back(dy > 0 ? TOUCH_SWIPE_UP : TOUCH_SWIPE_DOWN);
        } else {
          _maybe_gestures.push_back(TOUCH_LONG_PRESS);
        }
      }
    }
    _prev = p;
  }
  struct GestureAnalyzer {
    void operator()(TouchGesture g) {
      if (_at_start) {
        _at_start = false;
        _gesture = g;
      } else {
        if ( g != _gesture) {
          _gesture = TOUCH_OFF; // all detected gestures should be the same
        }
      }
    }
    bool _at_start{true};
    TouchGesture _gesture;
  };

  TouchGesture getGesture() const {
    GestureAnalyzer analyzer;
    std::for_each(_maybe_gestures.begin(), _maybe_gestures.end(), analyzer);
    return analyzer._gesture;
  }
  static constexpr auto THRESHOLD_X = 5;
  static constexpr auto THRESHOLD_Y = 5;
  bool _at_start { true };
  TS_Point _prev;
  std::vector<TouchGesture> _maybe_gestures;
};

void detectGesture()
{
  GestureDetector detector;
  std::for_each(touchedPoints.begin(), touchedPoints.end(), detector);
  touchedPoints.clear();
  auto gesture = detector.getGesture();
  if (gesture != TOUCH_OFF) {
    xTaskNotify(touchScreenTaskHandle, gesture, eSetBits);
  }
}

void touchTimer(void*)
{
  touched = false;
  detectGesture();
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

TS_Point getTouchPoint()
{
  auto p = ts.getPoint();
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, 240);
  return p;
}

void touch_screen_input(lv_indev_drv_t* drv, lv_indev_data_t* data)
{
  if (ts.tirqTouched()) {
    if (ts.touched()) {
      vTaskDelay(pdMS_TO_TICKS(30)); // debounce the touch gesture
      if (ts.touched()) {
        resetTouchTimer();
        auto p = getTouchPoint();
        if (!touched) {
          touched = true; // avoid sending multiple touched events
          ESP_LOGD(TAG, "Touched at %d,%d with pressure %d", p.x, p.y, p.z);
          oldPoint = p;
          data->state = LV_INDEV_STATE_PR;
          data->point.x = p.x;
          data->point.y = p.y;

          xTaskNotify(touchScreenTaskHandle, TOUCH_ON, eSetBits);
          return;
        } else {
          // we are already being touched, try and detect gestures
          touchedPoints.push_back(p);
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
          events.postTouchedEvent(touchedPoint);
          vTaskDelay(pdMS_TO_TICKS(1000)); // wait a bit to avoid repeat
                                           // sending for same touch event
          xTaskNotifyStateClear(NULL);
        }
      }
    }
  }
}
