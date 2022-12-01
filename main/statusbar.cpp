
#include "statusbar.h"
#include <esp_wifi.h>

lv_obj_t *time_label = nullptr;
lv_obj_t *status_label = nullptr;

StatusBar::StatusBar(lv_obj_t *parent) {
  static lv_style_t status_style;
  lv_style_init(&status_style);
  lv_style_set_text_color(&status_style, lv_palette_darken(LV_PALETTE_AMBER, 4));
  lv_style_set_text_font(&status_style, &lv_font_montserrat_16);

  time_label = lv_label_create(parent);
  lv_obj_add_style(time_label, &status_style, 0);
  lv_label_set_long_mode(time_label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(time_label, "??:??");
  lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(time_label, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_add_flag(time_label, LV_OBJ_FLAG_HIDDEN);

  status_label = lv_label_create(parent);
  lv_obj_add_style(status_label, &status_style, 0);
  lv_label_set_text(status_label, LV_SYMBOL_WIFI);
  lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_flag(status_label, LV_OBJ_FLAG_HIDDEN);

}


int8_t getWifiQuality() {
  wifi_ap_record_t ap_info;
  ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(&ap_info));
  auto dbm = ap_info.rssi;
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
  time_t now = ::time(nullptr);
  struct tm * timeinfo = localtime(&now);
  lv_label_set_text_fmt(time_label, "%2d:%02d",timeinfo->tm_hour, timeinfo->tm_min);

  lv_label_set_text_fmt(status_label, LV_SYMBOL_WIFI " %d%% ",
      getWifiQuality());

  if (firstUpdate) {
    lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
  }

}
