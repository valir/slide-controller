
#include <MQ135.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include "dht.h"

static const char* TAG = "AIR";

enum MQ135_Status {
  MQ135_STATUS_OK,
  MQ135_STATUS_UNCORRECTED,
  MQ135_STATUS_INVALID
};

struct MQ135_Info {
  float ppm = 0.0;
  uint64_t polling_interval = pdMS_TO_TICKS(10000);
  MQ135_Status status = MQ135_STATUS_INVALID;
};


MQ135_Info mq135_info;

void mq135_task(void*) {
  MQ135 mq135(GPIO_NUM_36);

  vTaskDelay(pdMS_TO_TICKS(60000)); // MQ135 requires at least 1min warm-up

  for (;;){
    vTaskDelay(mq135_info.polling_interval);
    if (dht_info.status == DHT_STATUS_OK) {
      mq135_info.ppm = mq135.getCorrectedPPM(dht_info.temperature, dht_info.relative_humidity);
      mq135_info.status = MQ135_STATUS_OK;
    } else {
      mq135_info.ppm = mq135.getPPM();
      mq135_info.status = MQ135_STATUS_UNCORRECTED;
    }
    ESP_LOGI(TAG, "%5.2f ppm %s", mq135_info.ppm, mq135_info.status == MQ135_STATUS_OK ? "corrected" : "uncorrected");
  }

  vTaskDelete(NULL);
}

