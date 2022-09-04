
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_task_wdt.h>
#include <esp_freertos_hooks.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp32-hal-gpio.h>
#include <SPI.h>

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
static void beep_stop(void*) {
  ESP_ERROR_CHECK(gpio_set_level(beep_pin, 0));
}

void beep_start(uint16_t ms) {
  ESP_ERROR_CHECK(gpio_set_level(beep_pin, 1));
  ESP_ERROR_CHECK(gpio_set_direction(beep_pin, GPIO_MODE_OUTPUT));
  ESP_ERROR_CHECK(gpio_pullup_en(beep_pin));
  esp_timer_create_args_t timer_args = {
    .callback = beep_stop,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1
  };
  esp_timer_handle_t timer_handle;
  ESP_ERROR_CHECK( esp_timer_create(&timer_args, &timer_handle) );
  ESP_ERROR_CHECK( esp_timer_start_once(timer_handle, ms*1000) );
}

extern "C" void app_main() {

  // initArduino();
  nvs_flash_init();
  display_allocate_heap(); // allocate early so we get that large chunk of DMA-capable RAM
  beep_start(300);
  // auto err = esp_register_freertos_idle_hook_for_cpu(idle_hook, 0);
  // if (ESP_OK != err) {
  //   printf("Error registering idle hook %d", err);
  // }

  ESP_LOGI(TAG, "Starting tasks...");
  // xTaskCreatePinnedToCore(displayTask, "displayTask", 12288, NULL, 1, &displayTaskHandle, 1);
  // xTaskCreatePinnedToCore(wifiTask, "wifiTask", 8192, NULL, 1, &wifiTaskHandle, 1);
  xTaskCreatePinnedToCore(sensors_task, "sensorsTask", 8192, NULL, 2, &sensorsTaskHandle, 1);
  // xTaskCreatePinnedToCore(touchScreenTask, "touchTask", 8192, NULL, 1, &touchScreenTaskHandle, 1);
  // xTaskCreateUniversal(rs485_task, "rs485Task", 4096, NULL, 1, &rs485TaskHandle, 0);
}
