

#include <ILI9341_SPI.h>
#include <XPT2046_Touchscreen.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <lvgl/lvgl.h>
#include <time.h>

#include "display.h"
#include "hal/lv_hal_disp.h"
#include "mainpanel.h"
#if defined(CONFIG_HAS_INTERNAL_SENSOR) || defined(CONFIG_HAS_EXTERNAL_SENSOR)
#include "sensors.h"
#endif
#include "backlight.h"
#include "events.h"
#include "statusbar.h"

// Pins for the ILI9341 and ESP32
#define LCD_DC 4
#define LCD_CS 5
#define LCD_LED 15
#define LCD_RST 22

static const char* TAG = "DISPLAY";
constexpr int SCREEN_WIDTH = 240;
constexpr int SCREEN_HEIGHT = 320;
constexpr int BITS_PER_PIXEL = 16; // this must be in sync with LV_COLOR_DEPTH
constexpr size_t BUFFER_SIZE
    = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t) / 4;

ILI9341_SPI lcd = ILI9341_SPI(LCD_CS, LCD_DC, LCD_RST);
#define LV_TICK_PERIOD_MS 10

static void lv_tick_task(void*) { lv_tick_inc(LV_TICK_PERIOD_MS); }

lv_color_t* buf1 = nullptr;

struct TouchedAnimation {
  lv_obj_t* _anim_circle = nullptr;
  void start(lv_point_t p)
  {
    if (_anim_circle != nullptr) {
      lv_obj_del(_anim_circle);
    }
    // TODO create the circle as an lv_arc here
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_exec_cb(&a, anim_func);
    lv_anim_set_var(&a, _anim_circle);
    lv_anim_set_time(&a, 1500);
  }
  static void anim_func(void*, int32_t) {}
};

struct DisplayEventObserver : public EventObserver {
  virtual void notice(const Event& event) override
  {
    xTaskNotify(displayTaskHandle, DISPLAY_UPDATE_WIDGETS, eSetBits);
    if (event.event == EVENT_SCREEN_TOUCHED) {
    }
  }
  virtual const char* name() override { return "display"; }
};

static DisplayEventObserver displayEventObserver;

void display_allocate_heap()
{
  heap_caps_print_heap_info(MALLOC_CAP_DMA);
  // alloc draw buffers used by LVGL
  // it's recommended to choose the size of the draw buffer(s) to be at least
  // 1/10 screen sized
  ESP_LOGI(
      TAG, "Attempting to allocate buffer of size %u bytes", BUFFER_SIZE);
  buf1 = (lv_color_t*)heap_caps_malloc(
      BUFFER_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf1);
}

void display_flush(
    lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p)
{
  const uint32_t w = (area->x2 - area->x1 + 1);
  const uint32_t h = (area->y2 - area->y1 + 1);
  BufferInfo bi;
  bi.buffer = (uint8_t*)color_p, bi.bitsPerPixel = BITS_PER_PIXEL,
  bi.palette
      = nullptr, /* palette is not needed with 16 bits/pixel color depth */
      bi.windowX = area->x1, bi.windowY = area->y1, bi.windowHeight = h,
  bi.windowWidth = w, bi.targetX = area->x1, bi.targetY = area->y1,
  bi.bufferWidth = w, bi.bufferHeight = h, bi.isPartialUpdate = true;

  lcd.writeBuffer(&bi);

  lv_disp_flush_ready(disp);
}

void touch_screen_input(lv_indev_drv_t* drv, lv_indev_data_t* data);

lv_obj_t* btn1;
lv_obj_t* btn2;
lv_obj_t* label;

// static void event_handler_btn(lv_obj_t *obj, lv_event_t event) {
//
// }

void create_temp_display() { }

lv_obj_t* aqi = NULL;

void create_aqi_display()
{
}

void setDisplayBacklight(bool on) { }

void displayTask(void*)
{
  // esp_task_wdt_add(NULL);
  lcd.init();
  lcd.setRotation(3);
  // turn on LCD background light
  BackLight::setPin(LCD_LED);
  BackLight::turnOn();

  lv_init();

  // create LGVL tick task
  esp_timer_handle_t lvgl_tick_timer = NULL;
  esp_timer_create_args_t timer_args = { .callback = lv_tick_task,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "lvgl_tick",
    .skip_unhandled_events = true };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(lvgl_tick_timer, LV_TICK_PERIOD_MS * 1000));

  // create some LGVG objects
  static lv_disp_draw_buf_t disp_buf;
  lv_disp_drv_t disp_drv;
  lv_indev_drv_t indev_drv;

  lv_disp_draw_buf_init(&disp_buf, buf1, nullptr, BUFFER_SIZE);

  /*Initialize the display*/

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_HEIGHT; // this is because we rotate screen in
                                    // hardware, see setRotation above
  disp_drv.ver_res = SCREEN_WIDTH;
  disp_drv.flush_cb = display_flush;
  disp_drv.draw_buf = &disp_buf;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the input device driver*/

  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_screen_input;
  lv_indev_drv_register(&indev_drv);

  /*Create screen objects*/

  StatusBar status_bar(lv_scr_act());
  MainPanel main_panel(lv_scr_act());

  events.registerObserver(&displayEventObserver);

  // label = lv_label_create(lv_scr_act());
  // lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  // lv_label_set_text(label, "Press a button");
  // lv_obj_set_size(label, 240, 40);
  // lv_obj_set_pos(label, 0, 15);
  //
  // // create_aqi_display();
  // // create_temp_display();
  //
  // btn1 = lv_btn_create(lv_scr_act());
  // // lv_obj_set_event_cb(btn1, event_handler_btn);
  // lv_obj_set_width(btn1, 70);
  // lv_obj_set_height(btn1, 32);
  // lv_obj_set_pos(btn1, 32, 100);
  //
  // lv_obj_t * label1 = lv_label_create(btn1);
  // lv_label_set_text(label1, "Hello");
  //
  // btn2 = lv_btn_create(lv_scr_act());
  // // lv_obj_set_event_cb(btn2, event_handler_btn);
  // lv_obj_set_width(btn2, 70);
  // lv_obj_set_height(btn2, 32);
  // lv_obj_set_pos(btn2, 142, 100);
  //
  // lv_obj_t * label2 = lv_label_create(btn2);
  // lv_label_set_text(label2, "Goodbye");

  lv_scr_load(lv_scr_act());
  auto last_minute = (time(0) % 3600) / 60;

  for (;;) {
    if ((lv_disp_get_inactive_time(NULL) < 1000)
#if defined(CONFIG_HAS_INTERNAL_SENSOR) || defined(CONFIG_HAS_EXTERNAL_SENSOR)
        || (!sensors_info.is_running())
#else
        || true
#endif
    ) {
      lv_task_handler();
    } else {
      // ESP_ERROR_CHECK(esp_timer_stop(lvgl_tick_timer));
      // sleep();
      // ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer,
      // LV_TICK_PERIOD_MS * 1000));
    }
    uint32_t notif_flags = 0;
    if (pdTRUE
        == xTaskNotifyWait(0x0, ULONG_MAX, &notif_flags, pdMS_TO_TICKS(10))) {
      if (notif_flags & DISPLAY_NOTIFY_TOUCH) {
        lv_task_handler();
      }
      auto current_minute = (time(0) % 3600) / 60;
      if ((notif_flags & DISPLAY_UPDATE_WIDGETS)
          || (last_minute != current_minute)) {
        last_minute = current_minute;
        status_bar.update();
        main_panel.update();
        lv_task_handler();
      }
    }
  }
}
