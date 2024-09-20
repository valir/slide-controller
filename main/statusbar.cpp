
#include "statusbar.h"
#include <esp_wifi.h>

static lv_obj_t* time_label = nullptr;
static lv_obj_t* status_label = nullptr;
static lv_obj_t* version_label = nullptr;
static lv_obj_t* label_hostname = nullptr;

#ifndef SC_VERSION
#error SC_VERSION is not defined
#endif

#define STR(n) #n
#define VERSION_AS_TEXT(n) STR(n)

StatusBar::StatusBar(lv_obj_t* parent)
{
  static lv_style_t status_style;
  lv_style_init(&status_style);
  lv_style_set_text_color(
      &status_style, lv_palette_darken(LV_PALETTE_AMBER, 4));
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

  static lv_style_t version_style;
  lv_style_init(&version_style);
  lv_style_set_text_color(
      &version_style, lv_palette_darken(LV_PALETTE_AMBER, 4));
  version_label = lv_label_create(parent);
  lv_obj_add_style(version_label, &version_style, 0);
  lv_label_set_long_mode(version_label, LV_LABEL_LONG_CLIP);
  lv_label_set_text(version_label, VERSION_AS_TEXT(SC_VERSION));
  lv_obj_set_style_text_align(version_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_align(version_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

  label_hostname = lv_label_create(parent);
  lv_label_set_text(label_hostname, CONFIG_HOSTNAME);
  lv_obj_align(label_hostname, LV_ALIGN_BOTTOM_MID, 0, 0);
  static lv_style_t label_hostname_style;
  lv_style_init(&label_hostname_style);
  lv_style_set_text_font(&label_hostname_style, &lv_font_montserrat_48);
  lv_style_set_text_color(
      &label_hostname_style, lv_palette_darken(LV_PALETTE_AMBER, 4));
  lv_obj_add_style(label_hostname, &label_hostname_style, 0);
  lv_obj_add_flag(label_hostname, LV_OBJ_FLAG_HIDDEN);
}

int8_t getWifiQuality()
{
  wifi_ap_record_t ap_info;
  auto rc = esp_wifi_sta_get_ap_info(&ap_info);
  auto dbm = ap_info.rssi;
  if (ESP_OK == rc) {
    if (dbm <= -100) {
      return 0;
    } else if (dbm >= -50) {
      return 100;
    } else {
      return 2 * (dbm + 100);
    }
  } else {
    return 0;
  }
}

void StatusBar::update()
{
  static bool firstUpdate = true;
  time_t now = ::time(nullptr);
  struct tm* timeinfo = localtime(&now);
  lv_label_set_text_fmt(
      time_label, "%2d:%02d", timeinfo->tm_hour, timeinfo->tm_min);

  auto wifi_qual = getWifiQuality();
  if (wifi_qual)
    lv_label_set_text_fmt(
        status_label, LV_SYMBOL_WIFI " %d%% ", wifi_qual);
  else
    lv_label_set_text(
        status_label, LV_SYMBOL_WIFI " ? " );

  if (firstUpdate) {
    lv_obj_clear_flag(time_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(status_label, LV_OBJ_FLAG_HIDDEN);
  }
}
