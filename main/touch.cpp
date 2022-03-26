

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <XPT2046_Touchscreen.h>


#define TOUCH_CS 14
//#define TOUCH_IRQ 2 // enable this line for older ArduiTouch or AZ-Touch ESP pcb
#define TOUCH_IRQ 27 // enable this line for new AZ-Touch MOD pcb

#define HAVE_TOUCHPAD

#define TS_MINX 370
#define TS_MINY 470
#define TS_MAXX 3700
#define TS_MAXY 3600

static const char* TAG = "TOUCH";

void touchScreenTask(void*) {
  XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
  if ( !ts.begin() ) {
    ESP_LOGE(TAG, "Cannot setup touchscreen");
    vTaskDelete(NULL);
    return;
  }
  ts.setRotation(3);

  for (;;){
    if (ts.tirqTouched()){
      if (ts.touched()) {
        vTaskDelay(pdMS_TO_TICKS(50));
        if (ts.touched()){
          auto p = ts.getPoint();
          p.x = map(p.x, TS_MINX, TS_MAXX, 0, 320);
          p.y = map(p.y, TS_MINY, TS_MAXX, 240, 0);
          ESP_LOGI(TAG, "Touched at %d,%d with pressure %d", p.x, p.y, p.z);
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
