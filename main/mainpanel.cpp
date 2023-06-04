#include <freertos/FreeRTOS.h>
#include <lvgl.h>
#include <math.h>

#include "backlight.h"
#include "mainpanel.h"

static lv_obj_t* init_spinner = nullptr;
static lv_obj_t* img_bulb = nullptr;
static lv_obj_t* temp_panel = nullptr;
static lv_obj_t* temp_label = nullptr;
static lv_obj_t* co2_panel = nullptr;
static lv_obj_t* co2_label = nullptr;
static lv_obj_t* humidity_panel = nullptr;
static lv_obj_t* humidity_label = nullptr;
static lv_obj_t* iaq_panel = nullptr;
static lv_obj_t* iaq_label = nullptr;
static lv_obj_t* ext_temp_panel = nullptr;
static lv_obj_t* ext_temp_label = nullptr;
static lv_obj_t* ext_humidity_panel = nullptr;
static lv_obj_t* ext_humidity_label = nullptr;

LV_FONT_DECLARE(monofur);

void create_temp_panel(lv_obj_t* parent, int x, int y, int w, int h)
{
  static lv_style_t style;
  lv_style_init(&style);
  // lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_bg_opa(&style, LV_OPA_TRANSP);
  lv_style_set_radius(&style, 0);
  lv_style_set_border_width(&style, 0);
  lv_style_set_pad_all(&style, 0);
  lv_style_set_text_font(&style, &lv_font_montserrat_48);
  lv_style_set_text_color(&style, lv_palette_darken(LV_PALETTE_AMBER, 4));

  temp_panel = lv_obj_create(parent);
  lv_obj_add_style(temp_panel, &style, 0);
  lv_obj_set_size(temp_panel, w, h);
  lv_obj_align(temp_panel, LV_ALIGN_TOP_LEFT, x, y);

  temp_label = lv_label_create(temp_panel);
  lv_label_set_text(temp_label, "?");
  lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_flag(temp_panel, LV_OBJ_FLAG_HIDDEN);
}

void create_ext_temp_panel(lv_obj_t* parent, int x, int y, int w, int h)
{
  static lv_style_t style;
  lv_style_init(&style);
  // lv_style_set_bg_color(&style, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_bg_opa(&style, LV_OPA_TRANSP);
  lv_style_set_radius(&style, 0);
  lv_style_set_border_width(&style, 0);
  lv_style_set_pad_all(&style, 0);
  lv_style_set_text_font(&style, &lv_font_montserrat_32);
  lv_style_set_text_color(&style, lv_palette_darken(LV_PALETTE_AMBER, 4));

  ext_temp_panel = lv_obj_create(parent);
  lv_obj_add_style(ext_temp_panel, &style, 0);
  lv_obj_set_size(ext_temp_panel, w, h);
  lv_obj_align(ext_temp_panel, LV_ALIGN_TOP_LEFT, x, y);

  ext_temp_label = lv_label_create(ext_temp_panel);
  lv_label_set_text(ext_temp_label, "?");
  lv_obj_align(ext_temp_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_flag(ext_temp_panel, LV_OBJ_FLAG_HIDDEN);
}

static lv_style_t co2_base_style;
static lv_style_t co2_warn_style;
static lv_style_t co2_high_style;
static lv_style_t co2_normal_style;
void create_co2_panel(lv_obj_t* parent, int x, int y, int w, int h)
{
  lv_style_init(&co2_base_style);
  // lv_style_set_bg_color(&co2_base_style,
  // lv_palette_main(LV_PALETTE_GREEN));
  lv_style_set_bg_opa(&co2_base_style, LV_OPA_TRANSP);
  lv_style_set_radius(&co2_base_style, 0);
  lv_style_set_border_width(&co2_base_style, 0);
  lv_style_set_pad_all(&co2_base_style, 0);
  lv_style_set_text_font(&co2_base_style, &lv_font_montserrat_32);

  co2_panel = lv_obj_create(parent);
  lv_obj_add_style(co2_panel, &co2_base_style, 0);
  lv_obj_set_size(co2_panel, w, h);
  lv_obj_align(co2_panel, LV_ALIGN_TOP_LEFT, x, y);

  lv_style_init(&co2_warn_style);
  lv_style_set_bg_color(
      &co2_warn_style, lv_palette_lighten(LV_PALETTE_AMBER, 3));
  lv_style_set_text_color(
      &co2_warn_style, lv_palette_darken(LV_PALETTE_AMBER, 2));
  lv_style_set_bg_opa(&co2_warn_style, LV_OPA_COVER);

  lv_style_init(&co2_high_style);
  lv_style_set_bg_color(
      &co2_high_style, lv_palette_darken(LV_PALETTE_RED, 2));
  lv_style_set_text_color(
      &co2_high_style, lv_palette_lighten(LV_PALETTE_RED, 2));
  lv_style_set_bg_opa(&co2_high_style, LV_OPA_COVER);

  lv_style_init(&co2_normal_style);
  lv_style_set_text_color(
      &co2_normal_style, lv_palette_main(LV_PALETTE_GREEN));
  lv_style_set_bg_opa(&co2_normal_style, LV_OPA_TRANSP);

  co2_label = lv_label_create(co2_panel);
  lv_label_set_text(co2_label, "?");
  lv_obj_align(co2_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_flag(co2_panel, LV_OBJ_FLAG_HIDDEN);
}

static lv_style_t humidity_base_style;
static lv_style_t humidity_low_style;
static lv_style_t humidity_high_style;
static lv_style_t humidity_normal_style;
void create_humidity_panel(lv_obj_t* parent, int x, int y, int w, int h)
{
  lv_style_init(&humidity_base_style);
  lv_style_set_bg_opa(&humidity_base_style, LV_OPA_TRANSP);
  lv_style_set_radius(&humidity_base_style, 0);
  lv_style_set_border_width(&humidity_base_style, 0);
  lv_style_set_pad_all(&humidity_base_style, 0);
  lv_style_set_text_font(&humidity_base_style, &lv_font_montserrat_32);

  humidity_panel = lv_obj_create(parent);
  lv_obj_add_style(humidity_panel, &humidity_base_style, 0);
  lv_obj_set_size(humidity_panel, w, h);
  lv_obj_align(humidity_panel, LV_ALIGN_TOP_LEFT, x, y);

  lv_style_init(&humidity_low_style);
  lv_style_set_bg_color(
      &humidity_low_style, lv_palette_darken(LV_PALETTE_RED, 3));
  lv_style_set_text_color(
      &humidity_low_style, lv_palette_lighten(LV_PALETTE_RED, 2));
  lv_style_set_bg_opa(&humidity_low_style, LV_OPA_COVER);

  lv_style_init(&humidity_high_style);
  lv_style_set_bg_color(
      &humidity_low_style, lv_palette_darken(LV_PALETTE_RED, 3));
  lv_style_set_text_color(
      &humidity_low_style, lv_palette_lighten(LV_PALETTE_RED, 2));
  lv_style_set_bg_opa(&humidity_high_style, LV_OPA_COVER);

  lv_style_init(&humidity_normal_style);
  lv_style_set_text_color(
      &humidity_normal_style, lv_palette_main(LV_PALETTE_GREEN));
  lv_style_set_bg_opa(&humidity_normal_style, LV_OPA_TRANSP);

  humidity_label = lv_label_create(humidity_panel);
  lv_label_set_text(humidity_label, "?");
  lv_obj_align(humidity_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_flag(humidity_panel, LV_OBJ_FLAG_HIDDEN);
}

static lv_style_t ext_humidity_base_style;
static lv_style_t ext_humidity_normal_style;
void create_ext_ext_humidity_panel(
    lv_obj_t* parent, int x, int y, int w, int h)
{
  lv_style_init(&ext_humidity_base_style);
  lv_style_set_bg_opa(&ext_humidity_base_style, LV_OPA_TRANSP);
  lv_style_set_radius(&ext_humidity_base_style, 0);
  lv_style_set_border_width(&ext_humidity_base_style, 0);
  lv_style_set_pad_all(&ext_humidity_base_style, 0);
  lv_style_set_text_font(&ext_humidity_base_style, &lv_font_montserrat_32);

  ext_humidity_panel = lv_obj_create(parent);
  lv_obj_add_style(ext_humidity_panel, &ext_humidity_base_style, 0);
  lv_obj_set_size(ext_humidity_panel, w, h);
  lv_obj_align(ext_humidity_panel, LV_ALIGN_TOP_LEFT, x, y);

  lv_style_init(&ext_humidity_normal_style);
  lv_style_set_text_color(
      &ext_humidity_normal_style, lv_palette_main(LV_PALETTE_GREEN));
  lv_style_set_bg_opa(&ext_humidity_normal_style, LV_OPA_TRANSP);

  ext_humidity_label = lv_label_create(ext_humidity_panel);
  lv_label_set_text(ext_humidity_label, "?");
  lv_obj_align(ext_humidity_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_flag(ext_humidity_panel, LV_OBJ_FLAG_HIDDEN);
}

static lv_style_t iaq_base_style;
static lv_style_t iaq_low_style;
static lv_style_t iaq_high_style;
static lv_style_t iaq_warn_style;
void create_iaq_panel(lv_obj_t* parent, int x, int y, int w, int h)
{
  lv_style_init(&iaq_base_style);
  // lv_style_set_bg_color(&iaq_base_style, lv_palette_main(LV_PALETTE_RED));
  lv_style_set_bg_opa(&iaq_base_style, LV_OPA_TRANSP);
  lv_style_set_radius(&iaq_base_style, 0);
  lv_style_set_border_width(&iaq_base_style, 0);
  lv_style_set_pad_all(&iaq_base_style, 0);
  lv_style_set_text_font(&iaq_base_style, &lv_font_montserrat_32);

  iaq_panel = lv_obj_create(parent);
  lv_obj_add_style(iaq_panel, &iaq_base_style, 0);
  lv_obj_set_size(iaq_panel, w, h);
  lv_obj_align(iaq_panel, LV_ALIGN_TOP_LEFT, x, y);

  lv_style_init(&iaq_low_style);
  lv_style_set_text_color(&iaq_low_style, lv_palette_main(LV_PALETTE_GREEN));
  lv_style_set_bg_opa(&iaq_low_style, LV_OPA_TRANSP);

  lv_style_init(&iaq_high_style);
  lv_style_set_bg_color(
      &iaq_high_style, lv_palette_darken(LV_PALETTE_RED, 2));
  lv_style_set_text_color(
      &iaq_high_style, lv_palette_lighten(LV_PALETTE_RED, 3));
  lv_style_set_bg_opa(&iaq_high_style, LV_OPA_COVER);

  lv_style_init(&iaq_warn_style);
  lv_style_set_bg_color(
      &iaq_warn_style, lv_palette_lighten(LV_PALETTE_AMBER, 3));
  lv_style_set_text_color(
      &iaq_warn_style, lv_palette_darken(LV_PALETTE_AMBER, 2));
  lv_style_set_bg_opa(&iaq_warn_style, LV_OPA_COVER);

  iaq_label = lv_label_create(iaq_panel);
  lv_label_set_text(iaq_label, "?");
  lv_obj_align(iaq_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_add_flag(iaq_panel, LV_OBJ_FLAG_HIDDEN);
}

void create_spinner(lv_obj_t* parent)
{
  /* display a spinner during initial start-up */
  static lv_style_t spinner_style;
  lv_style_init(&spinner_style);
  lv_style_set_arc_color(&spinner_style, lv_palette_main(LV_PALETTE_AMBER));

  static lv_style_t indicator_style;
  lv_style_init(&indicator_style);
  lv_style_set_arc_color(
      &indicator_style, lv_palette_darken(LV_PALETTE_AMBER, 4));

  init_spinner = lv_spinner_create(parent, 2000, 60);
  lv_obj_add_style(init_spinner, &spinner_style, 0);
  lv_obj_add_style(init_spinner, &indicator_style, LV_PART_INDICATOR);
  lv_obj_set_size(init_spinner, 100, 100);
  lv_obj_center(init_spinner);
}

void create_light_bulb(lv_obj_t* parent)
{
  // light bulb
  static lv_style_t bulb_style;
  lv_style_init(&bulb_style);
  lv_style_set_img_recolor(
      &bulb_style, lv_palette_lighten(LV_PALETTE_AMBER, 5));
  lv_style_set_img_recolor_opa(&bulb_style, LV_OPA_50);
  LV_IMG_DECLARE(img_lightbulb_outline_src);
  img_bulb = lv_img_create(parent);
  lv_obj_add_style(img_bulb, &bulb_style, 0);
  lv_img_set_src(img_bulb, &img_lightbulb_outline_src);
  lv_obj_align(img_bulb, LV_ALIGN_CENTER, 0, -20);
  lv_obj_set_size(img_bulb, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_add_flag(img_bulb, LV_OBJ_FLAG_HIDDEN);
}

// TODO configure which palette we should use in the menuconfig
MainPanel::MainPanel(lv_obj_t* parent)
{
#if CONFIG_USE_SENSOR_BME680
  // |      | CO2 |
  // | TEMP | RH  |
  // |      | IAQ |
  create_temp_panel(parent, 0, 16, 2 * 320 / 3, 208);
  create_co2_panel(parent, 2 * 320 / 3, 16, 320 / 3, 70);
  create_humidity_panel(parent, 2 * 320 / 3, 86, 320 / 3, 70);
  create_iaq_panel(parent, 2 * 320 / 3, 156, 320 / 3, 70);

#elif CONFIG_HAS_EXTERNAL_SENSOR
  // |      | EXT_TEMP |
  // | TEMP | RH       |
  // |      | EXT_RH   |
  create_temp_panel(parent, 0, 16, 2 * 320 / 3, 208);
  create_ext_temp_panel(parent, 2 * 320 / 3, 16, 320 / 3, 70);
  create_humidity_panel(parent, 2 * 320 / 3, 86, 320 / 3, 70);
  create_ext_ext_humidity_panel(parent, 2 * 320 / 3, 156, 320 / 3, 70);

#else

  create_light_bulb(parent);

#endif

  create_spinner(parent);

  events.registerObserver(this);
}

void MainPanel::update()
{
  if (init_spinner) {
    lv_obj_del(init_spinner);
    init_spinner = nullptr;

    if (img_bulb != nullptr) {
      lv_obj_clear_flag(img_bulb, LV_OBJ_FLAG_HIDDEN);
    }
    if (temp_panel != nullptr) {
      lv_obj_clear_flag(temp_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (co2_panel != nullptr) {
      lv_obj_clear_flag(co2_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (humidity_panel != nullptr) {
      lv_obj_clear_flag(humidity_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (iaq_panel != nullptr) {
      lv_obj_clear_flag(iaq_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (ext_temp_panel != nullptr) {
      lv_obj_clear_flag(ext_temp_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (ext_humidity_panel != nullptr) {
      lv_obj_clear_flag(ext_humidity_panel, LV_OBJ_FLAG_HIDDEN);
    }

    // trigger delayed backlight turn-off so device won't heat-up
    BackLight::activateTimer();
  }
}

void MainPanel::setTemp(float temp)
{
  if (temp_label != nullptr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", temp);
    lv_label_set_text(temp_label, buf);
  }
}

void MainPanel::setExtTemp(float temp)
{
  if (ext_temp_label != nullptr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", temp);
    lv_label_set_text(ext_temp_label, buf);
  }
}

void MainPanel::setHumidity(float humidity)
{
  if (humidity_label != nullptr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f%%", humidity);
    lv_label_set_text(humidity_label, buf);

    lv_obj_remove_style(humidity_panel, &humidity_low_style, 0);
    lv_obj_remove_style(humidity_panel, &humidity_normal_style, 0);
    lv_obj_remove_style(humidity_panel, &humidity_high_style, 0);
    if (humidity < 30) {
      lv_obj_add_style(humidity_panel, &humidity_low_style, 0);
    } else if (humidity > 70) {
      lv_obj_add_style(humidity_panel, &humidity_high_style, 0);
    } else {
      lv_obj_add_style(humidity_panel, &humidity_normal_style, 0);
    }
  }
}

void MainPanel::setExtHumidity(float humidity)
{
  if (ext_humidity_label != nullptr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f%%", humidity);
    lv_label_set_text(ext_humidity_label, buf);
  }
}

void MainPanel::setCO2(float co2)
{
  if (co2_label != nullptr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", co2);
    lv_label_set_text(co2_label, buf);

    lv_obj_remove_style(co2_panel, &co2_normal_style, 0);
    lv_obj_remove_style(co2_panel, &co2_warn_style, 0);
    lv_obj_remove_style(co2_panel, &co2_high_style, 0);
    if (co2 < 800) {
      lv_obj_add_style(co2_panel, &co2_normal_style, 0);
    } else if (co2 > 1200) {
      lv_obj_add_style(co2_panel, &co2_high_style, 0);
    } else {
      lv_obj_add_style(co2_panel, &co2_warn_style, 0);
    }
  }
}

void MainPanel::setIAQ(float iaq)
{
  if (iaq_label != nullptr) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.0f", iaq);
    lv_label_set_text(iaq_label, buf);

    lv_obj_remove_style(iaq_panel, &iaq_low_style, 0);
    lv_obj_remove_style(iaq_panel, &iaq_warn_style, 0);
    lv_obj_remove_style(iaq_panel, &iaq_high_style, 0);
    if (iaq < 100) {
      lv_obj_add_style(iaq_panel, &iaq_low_style, 0);
    } else if (iaq > 200) {
      lv_obj_add_style(iaq_panel, &iaq_high_style, 0);
    } else {
      lv_obj_add_style(iaq_panel, &iaq_warn_style, 0);
    }
  }
}

void MainPanel::notice(const Event& event)
{
  switch (event.event) {
  case EVENT_SENSOR_TEMPERATURE:
    setTemp(event.air_temperature);
    break;
  case EVENT_SENSOR_HUMIDITY:
    setHumidity(event.air_humidity);
    break;
#ifdef CONFIG_HAS_EXTERNAL_SENSOR
  case EVENT_SENSOR_EXT_TEMPERATURE:
    setExtTemp(event.air_temperature); // event field is reused for external sensor
    break;
  case EVENT_SENSOR_EXT_HUMIDITY:
    setExtHumidity(event.air_humidity); // event field is reused for external sensor
    break;
#endif
  case EVENT_SENSOR_IAQ:
    setIAQ(event.air_iaq);
    break;
  case EVENT_SENSOR_CO2:
    setCO2(event.air_co2);
    break;
  default:
    // ignore other events
    break;
  }
}
