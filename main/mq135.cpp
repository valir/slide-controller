
#include <MQ135.h>
#include <driver/gpio.h>
#include <esp_log.h>
#include <freertos/task.h>

#include "mq135.h"
#include "dht.h"
#include "events.h"

static const char* TAG = "CO2";



MQ135_Info mq135_info;

void mq135_task(void*) {
  MQ135 mq135(GPIO_NUM_36);

  vTaskDelay(pdMS_TO_TICKS(60000)); // MQ135 requires at least 1min warm-up

  for (;;){
    auto res = xTaskNotifyWait(0, 0x01, nullptr, mq135_info.polling_interval);
    if (res == pdTRUE) {
      // we received a calibration notification
      mq135.calibrateRZero(mq135_info.cal_ppm, mq135_info.cal_temperature, mq135_info.cal_rel_humidity);
    } else {
      if (dht_info.status == DHT_STATUS_OK) {
        mq135_info.ppm = mq135.getCorrectedPPM(dht_info.temperature, dht_info.relative_humidity);
        mq135_info.status = MQ135_STATUS_OK;
      } else {
        mq135_info.ppm = mq135.getPPM();
        mq135_info.status = MQ135_STATUS_UNCORRECTED;
      }
      ESP_LOGI(TAG, "%5.2f ppm %s", mq135_info.ppm, mq135_info.status == MQ135_STATUS_OK ? "corrected" : "uncorrected");
      events.postMq135Event(mq135_info.ppm);
    }
  }

  vTaskDelete(NULL);
}

