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
#endif
}
