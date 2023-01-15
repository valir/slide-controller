

#include "backlight.h"
#include "display.h"
#include "events.h"
#include "buzzer.h"
#include <XPT2046_Touchscreen.h>
#include <algorithm>
#include <driver/timer.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <list>
#include <lvgl/lvgl.h>
#include <utility>
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

#define TOUCH_GESTURE_BIT ((uint32_t)0x1 << 15)
// clang-format off
enum TouchNotification {
  TOUCH_OFF = (TOUCH_GESTURE_BIT << 1),
  TOUCH_ON  = (TOUCH_GESTURE_BIT << 2),
  TOUCH_NOTIF_SHORT_PRESS = TOUCH_GESTURE_BIT | TOUCH_GESTURE_SHORT_PRESS,
  TOUCH_NOTIF_LONG_PRESS  = TOUCH_GESTURE_BIT | TOUCH_GESTURE_SHORT_PRESS,
  TOUCH_NOTIF_LEFT        = TOUCH_GESTURE_BIT | TOUCH_GESTURE_SWIPE_LEFT,
  TOUCH_NOTIF_RIGHT       = TOUCH_GESTURE_BIT | TOUCH_GESTURE_SWIPE_RIGHT,
  TOUCH_NOTIF_UP          = TOUCH_GESTURE_BIT | TOUCH_GESTURE_SWIPE_UP,
  TOUCH_NOTIF_DOWN        = TOUCH_GESTURE_BIT | TOUCH_GESTURE_SWIPE_DOWN
};
// clang-format on

bool screen_is_being_touched = false;
esp_timer_handle_t touchTimerHandle;
bool timerInitialized = false;
std::vector<TS_Point> touchedPoints;

struct GestureDetector {
  GestureDetector() { }
  GestureDetector(const GestureDetector& other)
      : _maybe_gestures(other._maybe_gestures)
  {
  }
  void operator()(const TS_Point& p)
  {
    if (_at_start) {
      _at_start = false;
    } else {
      auto dx = p.x - _prev.x;
      auto dy = p.y - _prev.y;
      dx = (std::abs(dx) > THRESHOLD_X) ? dx : 0;
      dy = (std::abs(dy) > THRESHOLD_Y) ? dy : 0;
      if (dx && dy) {
        _maybe_gestures.push_back(TOUCH_GESTURE_OFF);
      } else {
        if (dx) {
          _maybe_gestures.push_back(
              dx > 0 ? TOUCH_GESTURE_SWIPE_RIGHT : TOUCH_GESTURE_SWIPE_LEFT);
        } else if (dy) {
          _maybe_gestures.push_back(
              dy > 0 ? TOUCH_GESTURE_SWIPE_DOWN : TOUCH_GESTURE_SWIPE_UP);
        } else {
          _maybe_gestures.push_back(TOUCH_GESTURE_LONG_PRESS);
        }
      }
    }
    _prev = p;
  }
  struct GestureAnalyzer {
    GestureAnalyzer() { }
    GestureAnalyzer(const GestureAnalyzer& other)
        : _counts(other._counts)
    {
    }
    void operator()(TouchGesture g)
    {
      auto pos = std::find_if(_counts.begin(), _counts.end(),
          [&](const GestureCount& c) { return c.first == g; });
      if (pos == _counts.end()) {
        _counts.push_back(std::make_pair(g, 0));
      } else {
        (*pos).second++;
      }
    }
    TouchGesture getGesture()
    {
      std::sort(_counts.begin(), _counts.end(),
          [](GestureCount l, GestureCount r) { return l.second > r.second; });
      if (_counts.empty()) {
        return TOUCH_GESTURE_OFF;
      }
      auto g = _counts.front().first;
      if ((g == TOUCH_GESTURE_LONG_PRESS) && (_counts.front().second < 8))
        g = TOUCH_GESTURE_SHORT_PRESS;
      return g;
    }
    bool _at_start { true };
    typedef std::pair<TouchGesture, uint8_t> GestureCount;
    std::vector<GestureCount> _counts;
  };

  TouchGesture getGesture() const
  {
    auto analyzer = std::for_each(
        _maybe_gestures.begin(), _maybe_gestures.end(), GestureAnalyzer());
    return analyzer.getGesture();
  }
  static constexpr auto THRESHOLD_X = 10;
  static constexpr auto THRESHOLD_Y = 10;
  bool _at_start { true };
  TS_Point _prev;
  std::vector<TouchGesture> _maybe_gestures;
};

void detectGesture()
{
  auto detector = std::for_each(
      touchedPoints.begin(), touchedPoints.end(), GestureDetector());
  auto gesture = detector.getGesture();
  if (gesture != TOUCH_GESTURE_OFF) {
    ESP_LOGI(TAG, "Detected gesture %d", gesture);
    xTaskNotify(touchScreenTaskHandle, TOUCH_GESTURE_BIT | gesture, eSetBits);
    buzzer.swipeDetectedTone();
  }
  touchedPoints.clear();
}

void touchTimer(void*)
{
  screen_is_being_touched = false;
  detectGesture();
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
        touchedPoints.push_back(p);
        // ESP_LOGI(TAG, "Touched at %d,%d with pressure %d", p.x, p.y, p.z);
        if (!screen_is_being_touched) {
          screen_is_being_touched
              = true; // avoid sending multiple touched events
          oldPoint = p;
          data->state = LV_INDEV_STATE_PR;
          data->point.x = p.x;
          data->point.y = p.y;

          xTaskNotify(touchScreenTaskHandle, TOUCH_ON, eSetBits);
          return;
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
    ESP_LOGD(TAG, "notif_flags = 0x%jx", (uintmax_t)notif_flags);
    if (notif_flags == TOUCH_ON) {
      if (!BackLight::isOn()) {
        BackLight::turnOn();
        vTaskDelay(pdMS_TO_TICKS(
            1000)); // prevent unintentional touched event trigger
      }
    }
    if (TOUCH_GESTURE_BIT & notif_flags) {
      lv_point_t lastTouchedPoint = { .x = oldPoint.x, .y = oldPoint.y };
      // auto no_lvitem_hit = lv_scr_act() ==
      // lv_indev_search_obj(lv_scr_act(), &lastTouchedPoint);
      TouchGesture g = (TouchGesture)(notif_flags & 0x07);
      ESP_LOGD(TAG, "task gesture %d", g);
      events.postTouchedEvent(g, lastTouchedPoint);
    } else {
    }
  }
}
