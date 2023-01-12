#include <freertos/FreeRTOS.h>
#include <lvgl.h>
#include <math.h>

#include "mainpanel.h"
#include "backlight.h"

static lv_obj_t* init_spinner = nullptr;
static lv_obj_t* img_bulb = nullptr;
static lv_obj_t* label_bulb = nullptr;

// TODO configure which palette we should use in the menuconfig
MainPanel::MainPanel(lv_obj_t* parent)
{

  /* display a spinner during initial start-up */
  static lv_style_t spinner_style;
  lv_style_init(&spinner_style);
  lv_style_set_arc_color(&spinner_style, lv_palette_main(LV_PALETTE_AMBER));

  static lv_style_t indicator_style;
  lv_style_init(&indicator_style);
  lv_style_set_arc_color(&indicator_style, lv_palette_darken(LV_PALETTE_AMBER, 4));

  init_spinner = lv_spinner_create(parent, 2000, 60);
  lv_obj_add_style(init_spinner, &spinner_style, 0);
  lv_obj_add_style(init_spinner, &indicator_style, LV_PART_INDICATOR);
  lv_obj_set_size(init_spinner, 100, 100);
  lv_obj_center(init_spinner);

  // light bulb
  static lv_style_t bulb_style;
  lv_style_init(&bulb_style);
  lv_style_set_img_recolor(&bulb_style, lv_palette_lighten(LV_PALETTE_AMBER, 5));
  lv_style_set_img_recolor_opa(&bulb_style, LV_OPA_50);
  LV_IMG_DECLARE(img_lightbulb_outline_src);
  img_bulb = lv_img_create(parent);
  lv_obj_add_style(img_bulb, &bulb_style, 0);
  lv_img_set_src(img_bulb, &img_lightbulb_outline_src);
  lv_obj_align(img_bulb, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_size(img_bulb, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_add_flag(img_bulb, LV_OBJ_FLAG_HIDDEN);

  label_bulb = lv_label_create(parent);
  lv_label_set_text(label_bulb, CONFIG_HOSTNAME);
  lv_obj_align(label_bulb, LV_ALIGN_BOTTOM_MID, 0, 0);
  static lv_style_t label_bulb_style;
  lv_style_init(&label_bulb_style);
  lv_style_set_text_font(&label_bulb_style, &lv_font_montserrat_48);
  lv_style_set_text_color(&label_bulb_style, lv_palette_darken(LV_PALETTE_AMBER, 4));
  lv_obj_add_style(label_bulb, &label_bulb_style, 0);
  lv_obj_add_flag(label_bulb, LV_OBJ_FLAG_HIDDEN);
}

void MainPanel::update()
{
  if (init_spinner) {
    lv_obj_del(init_spinner);
    init_spinner = nullptr;

    lv_obj_clear_flag(img_bulb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(label_bulb, LV_OBJ_FLAG_HIDDEN);
    // trigger delayed backlight turn-off so device won't heat-up
    BackLight::activateTimer();
  }

}
