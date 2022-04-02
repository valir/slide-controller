#include <cstring>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <hal/gpio_types.h>

#include "dht.h"
#include "events.h"

#define DHT_PIN GPIO_NUM_32

static const char* TAG = "TEMP";

DHT_Info dht_info;

float temperature_calibration = -1.5f;
int dht_isr_call_count = 0;
uint8_t dht_isr_bit_count = 0;
uint8_t dht_isr_woke_count = 0;
uint8_t isr_data[40];
uint8_t* isr_pdata = nullptr;
uint64_t dht_pin_raise_time;
bool dht_wait_init_pulse = true;
bool dht_pin_bits_started = false;
uint8_t dht_byte = 0;
uint8_t dht_bits = 0;
extern TaskHandle_t dhtTaskHandle;

void clear_isr_state()
{
  dht_isr_call_count = 0;
  dht_isr_bit_count = 0;
  dht_isr_woke_count = 0;
  dht_wait_init_pulse = true;
  dht_pin_bits_started = false;
  dht_byte = 0;
  dht_bits = 0;
  isr_pdata = &isr_data[0];
}

void IRAM_ATTR dht_isr_handler(void*)
{
  UBaseType_t si = taskENTER_CRITICAL_FROM_ISR();
  BaseType_t taskWoken = pdFALSE;
  dht_isr_call_count++;
  auto dht_pin_level = gpio_get_level(DHT_PIN);
  if (0 == dht_pin_level) {
    if (dht_pin_bits_started) [[likely]] {
       *isr_pdata = (((esp_timer_get_time() - dht_pin_raise_time) > 50) ? 0x01 : 0x00);
       isr_pdata++;
    }
    dht_isr_bit_count++;
  } else {
    dht_pin_raise_time = esp_timer_get_time();
    // raising edge
    if (dht_wait_init_pulse) [[unlikely]] {
      dht_wait_init_pulse = false;
    } else [[likely]] {
      dht_pin_bits_started = true;
      if (dht_isr_bit_count == 40) {
        xTaskNotifyFromISR(dhtTaskHandle, 0x01, eSetBits, &taskWoken);
      }
    }
  }
  taskEXIT_CRITICAL_FROM_ISR(si);
  if (taskWoken) {
    dht_isr_woke_count++;
    portYIELD_FROM_ISR();
  }
}

void dht_task(void*)
{
  gpio_config_t io_config = {};
  io_config.pin_bit_mask = 1ULL << DHT_PIN;
  io_config.intr_type = GPIO_INTR_DISABLE;
  io_config.mode = GPIO_MODE_DISABLE;
  io_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  io_config.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK(gpio_config(&io_config));

  ESP_ERROR_CHECK(gpio_set_level(DHT_PIN, 1));
  ESP_ERROR_CHECK(
      gpio_install_isr_service(ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM));
  ESP_ERROR_CHECK(gpio_isr_handler_add(DHT_PIN, dht_isr_handler, NULL));
  ESP_ERROR_CHECK(gpio_set_intr_type(DHT_PIN, GPIO_INTR_ANYEDGE));

  ESP_LOGI(TAG, "Starting loop.");
  for (;;) {
    vTaskDelay(dht_info.polling_interval);

    // start reading
    ESP_ERROR_CHECK(gpio_set_level(DHT_PIN, 0));
    ESP_ERROR_CHECK(gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT_OD));
    vTaskDelay(1);
    xTaskNotifyStateClear(nullptr);
    ESP_ERROR_CHECK(gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_level(DHT_PIN, 1));
    clear_isr_state();
    ESP_ERROR_CHECK(gpio_intr_enable(DHT_PIN));

    if (pdTRUE == xTaskNotifyWait(0, 0x01, nullptr, pdMS_TO_TICKS(2500))) {
      char data[5];
      isr_pdata = &isr_data[0];
      for (int b = 0; b <5; b++) {
        char byte = 0;
        for (int bits=0; bits<8; bits++) {
          byte = (byte <<1) | *isr_pdata;
          isr_pdata++;
        }
        data[b] = byte;
      }
      if (data[4]
          == (((data[0] + data[1] + data[2] + data[3]))
              & 0xFF)) {
        ESP_LOGW(TAG, "Checksum FAILED");
      }
        dht_info.status = DHT_STATUS_OK;
        dht_info.relative_humidity
            = (((uint16_t)data[0]) << 8 | data[1]) * 0.1;
        dht_info.temperature
            = (((uint16_t)(data[2] & 0x7F)) << 8 | data[3]) * 0.1;
        dht_info.temperature += temperature_calibration;
        if (data[2] & 0x80)
          dht_info.temperature *= -1.0;
        ESP_LOGI(TAG, "%4.1f °C | %5.1f%% RH | dht_isr_call_count %d",
            dht_info.temperature, dht_info.relative_humidity,
            dht_isr_call_count);

        events.postDhtEvent(dht_info.temperature, dht_info.relative_humidity);
      // } else {
      //   ESP_LOGE(TAG, "Checksum FAILED");
      //   ESP_LOGI(TAG, "dht_isr_call_count = %d, dht_isr_woke_count = %d",
      //       dht_isr_call_count, dht_isr_woke_count);
      //   dht_info.status = DHT_STATUS_INVALID;
      //   // events.postDhtStatus(DHT_STATUS_INVALID);
      // }
    } else {
      ESP_LOGE(TAG, "Timeout while waiting for the data to come");
      ESP_LOGI(TAG, "dhtdht_isr_bit_count = %d", dht_isr_bit_count);
    }
  }

  vTaskDelete(NULL);
}
