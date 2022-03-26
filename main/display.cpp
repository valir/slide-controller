

#include "esp_task_wdt.h"
#include <MiniGrafx.h>
#include <ILI9341_SPI.h>

#include "mq135.h"
#include "dht.h"

// Pins for the ILI9341 and ESP32
#define LCD_DC 4
#define LCD_CS 5
#define LCD_LED 15
#define LCD_RST 22

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

void displayTask(void *) {
  esp_task_wdt_add(NULL);
  // turn on LCD background light
  pinMode(LCD_LED, OUTPUT);
  digitalWrite(LCD_LED, LOW);

  gfx.init();
  gfx.setRotation(3);
  gfx.fillBuffer(3);
  gfx.commit();

  for (;;) {
    // circleSize = ( circleSize + 1 ) % 5;
    gfx.fillBuffer(0);
    gfx.setColor(1);
    char string_buf[128];
    snprintf(string_buf, 128, "%5.2f °C RH %5.2f\n%5.2f ppm", dht_info.temperature, dht_info.relative_humidity, mq135_info.ppm);
    gfx.drawString(0, 0, string_buf);
    // gfx.drawLine(0, 0, 20, 20);
    // gfx.setColor(13);
    // gfx.fillCircle(20, 20, 5 + circleSize);
    gfx.commit();
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

