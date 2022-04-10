
#include <simpleDSTadjust.h>
#include "statusbar.h"
#include <WiFi.h>
#include "sensors.h"

extern simpleDSTadjust dstAdjusted;



lv_obj_t *time_label = nullptr;
lv_obj_t *status_label = nullptr;

StatusBar::StatusBar(lv_obj_t *parent) {
  time_label = lv_label_create(parent);
  lv_label_set_long_mode(time_label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(time_label, "??:??");
  lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);

  status_label = lv_label_create(parent);
  lv_label_set_recolor(status_label, true);
  lv_label_set_text(status_label, "#ff0000 " LV_SYMBOL_WIFI "? #");
  lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);

}


int8_t getWifiQuality() {
  int32_t dbm = WiFi.RSSI();
  if(dbm <= -100) {
      return 0;
  } else if(dbm >= -50) {
      return 100;
  } else {
      return 2 * (dbm + 100);
  }
}

void StatusBar::update() {
  static bool firstUpdate = true;
  char *dstAbbrev;
  time_t now = dstAdjusted.time(&dstAbbrev);
  // ESP_LOGI(TAG, "got time: %s", ctime(&now));
  struct tm * timeinfo = localtime(&now);
  lv_label_set_text_fmt(time_label, "%2d:%02d",timeinfo->tm_hour, timeinfo->tm_min);

  lv_label_set_text_fmt(status_label, "%s " LV_SYMBOL_WIFI " %d%% #",
      WiFi.status() == WL_CONNECTED ? "#00ff00 " : "#ff0000 ",
      getWifiQuality());

  if (firstUpdate) {
    lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
  }

}
