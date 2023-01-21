
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_freertos_hooks.h>
#include <esp_log.h>
#include <esp_pm.h>
#include <nvs_flash.h>
#include <esp32-hal-gpio.h>
#include <SPI.h>
#include <buzzer.h>


static const char* TAG = "MAIN";

TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t sensorsTaskHandle = NULL;
TaskHandle_t rs485TaskHandle = NULL;
TaskHandle_t touchScreenTaskHandle = NULL;
TaskHandle_t wifiTaskHandle = NULL;

bool idle_hook() {
  return true;
}

void display_allocate_heap();
void sensors_task(void*);
void rs485_task(void*);
void displayTask(void*);
void touchScreenTask(void*);
void wifiTask(void*);

constexpr gpio_num_t beep_pin = GPIO_NUM_21;

extern "C" void app_main() {

  // initArduino();
  auto err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);

  display_allocate_heap(); // allocate early so we get that large chunk of DMA-capable RAM
  buzzer.playStartup();
  // auto err = esp_register_freertos_idle_hook_for_cpu(idle_hook, 0);
  // if (ESP_OK != err) {
  //   printf("Error registering idle hook %d", err);
  // }

  esp_pm_config_esp32_t pm_config = {
    .max_freq_mhz = 240,
    .min_freq_mhz = 80,
    .light_sleep_enable = true
  };
  ESP_ERROR_CHECK( esp_pm_configure(&pm_config) );

  ESP_LOGI(TAG, "Starting tasks...");
  xTaskCreatePinnedToCore(displayTask, "displayTask", 12288, NULL, 1, &displayTaskHandle, 1);
  xTaskCreatePinnedToCore(wifiTask, "wifiTask", 8192, NULL, 1, &wifiTaskHandle, 0);

#if defined(CONFIG_HAS_EXTERNAL_SENSOR) || defined(CONFIG_HAS_INTERNAL_SENSOR)
  xTaskCreatePinnedToCore(sensors_task, "sensorsTask", 8192, NULL, 2, &sensorsTaskHandle, 1);
#endif
  xTaskCreatePinnedToCore(touchScreenTask, "touchTask", 8192, NULL, 1, &touchScreenTaskHandle, 1);
  // xTaskCreateUniversal(rs485_task, "rs485Task", 4096, NULL, 1, &rs485TaskHandle, 0);
}
