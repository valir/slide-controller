#include <freertos/FreeRTOS.h>
#include <lvgl.h>
#include <math.h>

#include "mainpanel.h"
#include "sensors.h"
#include "backlight.h"

static lv_obj_t* init_spinner = nullptr;
static lv_obj_t* temp_meter = nullptr;
static lv_meter_indicator_t* temp_actual = nullptr;
static lv_meter_indicator_t* temp_requested = nullptr;
static lv_obj_t* temp_label = nullptr;

static lv_obj_t* rh_meter = nullptr;
static lv_obj_t* rh_label = nullptr;

static lv_obj_t* co2_meter = nullptr;
static lv_obj_t* co2_label = nullptr;

MainPanel::MainPanel(lv_obj_t* parent)
{
  /* setup the temperature meter */
  temp_meter = lv_meter_create(parent);
  lv_obj_align(temp_meter, LV_ALIGN_LEFT_MID, 30, 0);
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

  /* setup the relative humidity meter */
  rh_meter = lv_bar_create(parent);
  lv_obj_set_size(rh_meter, 20, 100);
  lv_obj_align(rh_meter, LV_ALIGN_RIGHT_MID, -40, -40);
  lv_bar_set_value(rh_meter, 0, LV_ANIM_ON);
  lv_obj_add_flag(rh_meter, LV_OBJ_FLAG_HIDDEN);

  rh_label = lv_label_create(parent);
  lv_obj_align(rh_label, LV_ALIGN_RIGHT_MID, -40, 20);
  lv_obj_add_flag(rh_label, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(rh_label, "0 %");

  /* setup the CO2 meter using a LED and a label */
  co2_meter = lv_led_create(parent);
  lv_obj_align(co2_meter, LV_ALIGN_RIGHT_MID, -40, 50);
  lv_obj_add_flag(co2_meter, LV_OBJ_FLAG_HIDDEN);

  co2_label = lv_label_create(parent);
  lv_label_set_recolor(co2_label, true);
  lv_obj_align(co2_label, LV_ALIGN_RIGHT_MID, -40, 80);
  lv_obj_add_flag(co2_label, LV_OBJ_FLAG_HIDDEN);

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

#ifndef ENV_NO_SENSOR
    lv_obj_clear_flag(temp_meter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(rh_meter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(rh_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(co2_meter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(co2_label, LV_OBJ_FLAG_HIDDEN);
#endif

    // trigger delayed backlight turn-off so device won't heat-up
    BackLight::activateTimer();
  }

#ifndef ENV_NO_SENSOR
  lv_meter_set_indicator_end_value(
      temp_meter, temp_actual, sensors_info.temperature);
  static char txt_buffer[128];
  // snprintf(txt_buffer, sizeof(txt_buffer) / sizeof(txt_buffer[0]),
  //     "%s camera: %4.1f °C %c \n %s ceruta: %4.1f °C %c", "#00ff00",
  //     sensors_info.temperature, '#', "#0000ff", 0., '#');
  snprintf(txt_buffer, sizeof(txt_buffer) / sizeof(txt_buffer[0]),
      "%s %s: %4.1f °C %c", "#00ff00", ENV_HOSTNAME, sensors_info.temperature, '#');
  lv_label_set_text(temp_label, txt_buffer);

  snprintf(txt_buffer, sizeof(txt_buffer) / sizeof(txt_buffer[0]), "%4.1f %%",
      sensors_info.relative_humidity);
  lv_label_set_text(rh_label, txt_buffer);
  lv_bar_set_value(
      rh_meter, round(sensors_info.relative_humidity), LV_ANIM_ON);

  static auto constexpr IAQ_GOOD_MAX = 50.;
  static auto constexpr IAQ_AVERAGE_MAX = 100.;
  snprintf(txt_buffer, sizeof(txt_buffer) / sizeof(txt_buffer[0]),
      "%s %4.0f iaq %c",
      sensors_info.iaq <= IAQ_GOOD_MAX
          ? "#00ff00"
          : (sensors_info.iaq <= IAQ_AVERAGE_MAX ? "#ffff00" : "#ff0000"),
      sensors_info.iaq, '#');
  lv_label_set_text(co2_label, txt_buffer);
  lv_led_set_color(co2_meter,
      lv_palette_main(sensors_info.iaq <= IAQ_GOOD_MAX
              ? LV_PALETTE_GREEN
              : (sensors_info.iaq < IAQ_AVERAGE_MAX ? LV_PALETTE_YELLOW
                                                    : LV_PALETTE_RED)));
#endif
}
