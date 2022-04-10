#include <freertos/FreeRTOS.h>
#include <lvgl.h>

#include "mainpanel.h"
#include "sensors.h"

static lv_obj_t* init_spinner = nullptr;
static lv_obj_t* temp_meter = nullptr;
static lv_meter_indicator_t* temp_actual = nullptr;
static lv_meter_indicator_t* temp_requested = nullptr;
static lv_obj_t* temp_label = nullptr;

MainPanel::MainPanel(lv_obj_t* parent)
{
  temp_meter = lv_meter_create(parent);
  lv_obj_center(temp_meter);
  lv_obj_set_size(temp_meter, 200, 200);
  lv_obj_remove_style(temp_meter, NULL, LV_PART_INDICATOR);
  lv_obj_add_flag(temp_meter, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(temp_meter, LV_OBJ_FLAG_CLICKABLE);

  lv_meter_scale_t* temp_scale = lv_meter_add_scale(temp_meter);
  lv_meter_set_scale_ticks(
      temp_meter, temp_scale, 5, 2, 10, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(
      temp_meter, temp_scale, 1, 2, 30, lv_color_hex3(0xeee), 15);
  lv_meter_set_scale_range(temp_meter, temp_scale, 0, 40, 180, 180);

  temp_actual = lv_meter_add_arc(
      temp_meter, temp_scale, 10, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_meter_set_indicator_start_value(temp_meter, temp_actual, 0);
  lv_meter_set_indicator_end_value(temp_meter, temp_actual, 0);

  temp_requested = lv_meter_add_arc(temp_meter, temp_scale, 10,
      lv_palette_main(LV_PALETTE_LIGHT_BLUE), -10);
  lv_meter_set_indicator_start_value(temp_meter, temp_requested, 0);
  lv_meter_set_indicator_end_value(temp_meter, temp_requested, 0);

  temp_label = lv_label_create(temp_meter);
  lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, 30);
  lv_label_set_recolor(temp_label, true);

  /* display a spinner during initial start-up */
  init_spinner = lv_spinner_create(parent, 2000, 60);
  lv_obj_set_size(init_spinner, 100, 100);
  lv_obj_center(init_spinner);
}

void MainPanel::update()
{
  if (init_spinner) {
    lv_obj_del(init_spinner);
    init_spinner = nullptr;

    lv_obj_clear_flag(temp_meter, LV_OBJ_FLAG_HIDDEN);
  }

  lv_meter_set_indicator_end_value(
      temp_meter, temp_actual, dht_info.temperature);
  static char temp_label_txt[128];
  snprintf(temp_label_txt, sizeof(temp_label_txt)/sizeof(temp_label_txt[0]),
      "%s camera: %4.1f °C %c \n %s ceruta: %4.1f °C %c",
      "#00ff00", dht_info.temperature, '#',
      "#0000ff", 0., '#');
  lv_label_set_text(temp_label, temp_label_txt);
}
