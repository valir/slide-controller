#pragma once

#include <lvgl.h>

class StatusBar {
  public:
    StatusBar(lv_obj_t *parent);

    void update();
};
