
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_freertos_hooks.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <SPI.h>

static const char* TAG = "MAIN";

TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t dhtTaskHandle = NULL;
TaskHandle_t mq135TaskHandle = NULL;
TaskHandle_t rs485TaskHandle = NULL;
TaskHandle_t touchScreenTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;

bool idle_hook() {
  return true;
}

void display_allocate_heap();
void dht_task(void*);
void mq135_task(void*);
void rs485_task(void*);
void displayTask(void*);
void touchScreenTask(void*);
void wifiTask(void*);

extern "C" void app_main() {

  // initArduino();
  nvs_flash_init();
  display_allocate_heap(); // allocate early so we get that large chunk of DMA-capable RAM
  vTaskDelay(100);
  // auto err = esp_register_freertos_idle_hook_for_cpu(idle_hook, 0);
  // if (ESP_OK != err) {
  //   printf("Error registering idle hook %d", err);
  // }

  ESP_LOGI(TAG, "Starting tasks...");
  xTaskCreatePinnedToCore(displayTask, "displayTask", 8192, NULL, 1, &displayTaskHandle, 1);
  xTaskCreatePinnedToCore(dht_task, "dhtTask", 8192, NULL, 2, &dhtTaskHandle, 1);
  xTaskCreatePinnedToCore(mq135_task, "mq135Task", 4096, NULL, 1, &mq135TaskHandle, 1);
  xTaskCreatePinnedToCore(touchScreenTask, "touchTask", 8192, NULL, 1, &touchScreenTaskHandle, 1);
  xTaskCreatePinnedToCore(wifiTask, "wifiTask", 8192, NULL, 1, &wifiTaskHandle, 1);
  // xTaskCreateUniversal(rs485_task, "rs485Task", 4096, NULL, 1, &mq135TaskHandle, 0);
}
