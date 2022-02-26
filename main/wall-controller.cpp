
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "esp_freertos_hooks.h"
#include <SPI.h>
#include <MiniGrafx.h>
#include <ILI9341_SPI.h>
#include <XPT2046_Touchscreen.h>

// Pins for the ILI9341 and ESP32
#define LCD_DC 4
#define LCD_CS 5
#define LCD_LED 15
#define LCD_RST 22

#define HAVE_TOUCHPAD
#define TOUCH_CS 14
//#define TOUCH_IRQ 2 // enable this line for older ArduiTouch or AZ-Touch ESP pcb
#define TOUCH_IRQ 27 // enable this line for new AZ-Touch MOD pcb

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

uint16_t palette[] = {ILI9341_BLACK,     //  0
                      ILI9341_WHITE,     //  1
                      ILI9341_NAVY,      //  2
                      ILI9341_DARKCYAN,  //  3
                      ILI9341_DARKGREEN, //  4
                      ILI9341_MAROON,    //  5
                      ILI9341_PURPLE,    //  6
                      ILI9341_OLIVE,     //  7
                      ILI9341_LIGHTGREY, //  8
                      ILI9341_DARKGREY,  //  9
                      ILI9341_BLUE,      // 10
                      ILI9341_GREEN,     // 11
                      ILI9341_CYAN,      // 12
                      ILI9341_RED,       // 13
                      ILI9341_MAGENTA,   // 14
                      0xFD80};           // 15

int SCREEN_WIDTH = 240;
int SCREEN_HEIGHT = 320;
int BITS_PER_PIXEL = 4;

ILI9341_SPI lcd = ILI9341_SPI(LCD_CS, LCD_DC, LCD_RST);
MiniGrafx gfx = MiniGrafx(&lcd, BITS_PER_PIXEL, palette, SCREEN_WIDTH, SCREEN_HEIGHT);

int circleSize = 0;

TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t mq135TaskHandle = NULL;

void displayTask(void *) {
  esp_task_wdt_add(NULL);
  // turn on LCD background light
  pinMode(LCD_LED, OUTPUT);
  digitalWrite(LCD_LED, LOW);

  gfx.init();
  gfx.fillBuffer(3);
  gfx.commit();

  for (;;) {
    circleSize = ( circleSize + 1 ) % 5;
    gfx.fillBuffer(0);
    gfx.setColor(1);
    gfx.drawLine(0, 0, 20, 20);
    gfx.setColor(13);
    gfx.fillCircle(20, 20, 5 + circleSize);
    gfx.commit();
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

bool idle_hook() {
  displayTask(NULL);
  return true;
}

void dht_task(void*);
void mq135_task(void*);

extern "C" void app_main() {
  Serial.begin(115200);
  printf("setting-up\n");

  initArduino();
  // auto err = esp_register_freertos_idle_hook_for_cpu(idle_hook, 0);
  // if (ESP_OK != err) {
  //   printf("Error registering idle hook %d", err);
  // }

  xTaskCreateUniversal(displayTask, "displayTask", 8192, NULL, 1, &displayTaskHandle, 0);
  xTaskCreateUniversal(dht_task, "dhtTask", 8192, NULL, 2, NULL, 1);
  xTaskCreateUniversal(mq135_task, "mq135Task", 4096, NULL, 1, &mq135TaskHandle, 0);
}
