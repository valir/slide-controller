#include <freertos/FreeRTOS.h>
#include <lvgl.h>
#include <math.h>

#include "mainpanel.h"
#include "sensors.h"
#include "backlight.h"

static lv_obj_t* init_spinner = nullptr;

MainPanel::MainPanel(lv_obj_t* parent)
{
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
      "%s %s: %4.1f °C %c", "#00ff00", CONFIG_HOSTNAME, sensors_info.temperature, '#');
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
