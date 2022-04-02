#include <esp32DHT.h>
#include <esp_log.h>
#include "dht.h"
#include "events.h"

#define DHT_PIN GPIO_NUM_32

static const char* TAG = "TEMP";

DHT_Info dht_info;
float temperature_calibration = -1.5f;

void dht_task(void*)
{
  DHT22 sensor;
  sensor.setup(DHT_PIN);
  sensor.onData([](float h, float t) {
        dht_info.status = DHT_STATUS_OK;
        dht_info.relative_humidity = h;
        dht_info.temperature = t;
        dht_info.temperature += temperature_calibration;
        ESP_LOGI(TAG, "%4.1f °C | %5.1f%% RH",
            dht_info.temperature, dht_info.relative_humidity);
        events.postDhtEvent(dht_info.temperature, dht_info.relative_humidity);
      });
  sensor.onError([](uint8_t error){
        ESP_LOGW(TAG, "Sensor error: %d", error);
      });
  ESP_LOGI(TAG, "Starting loop.");
  for (;;) {
    vTaskDelay(dht_info.polling_interval);
    sensor.read();
  }
}
