#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <esp32DHT.h>
#include <MQ135.h>

#include "sensors.h"
#include "events.h"

#define DHT_PIN GPIO_NUM_32

static const char* TAG = "SENSORS";

DHT22 temp_sensor;
DHT_Info dht_info;
float temperature_calibration = -1.5f;

MQ135_Info mq135_info;
MQ135 mq135(GPIO_NUM_36);

static void dht_data_recv(float h, float t)
{
  dht_info.status = DHT_STATUS_OK;
  dht_info.relative_humidity = h;
  dht_info.temperature = t;
  dht_info.temperature += temperature_calibration;
  ESP_LOGI(TAG, "%4.1f °C | %5.1f%% RH", dht_info.temperature,
      dht_info.relative_humidity);
  events.postDhtEvent(dht_info.temperature, dht_info.relative_humidity);

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

static void dht_error(uint8_t error)
{
  ESP_LOGW(TAG, "Sensor error: %d", error);
}

static void dht_periodic_read(void*) {
  temp_sensor.read();
}

void sensors_task(void*)
{
  vTaskDelay(pdMS_TO_TICKS(60000)); // wait for the sensors to settle

  temp_sensor.setup(DHT_PIN);
  temp_sensor.onData(dht_data_recv);
  temp_sensor.onError(dht_error);


  esp_timer_create_args_t timer_args = {
    .callback = dht_periodic_read,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1
  };
  esp_timer_handle_t timer_handle;
  ESP_ERROR_CHECK( esp_timer_create(&timer_args, &timer_handle) );
  ESP_ERROR_CHECK( esp_timer_start_periodic(timer_handle, 30000000) );
  vTaskDelete(nullptr);
}

void handleCalibrateAirQuality(const char *data, int data_len) {
  // payload contains 1 float, the calibrated ppm, as the temperature and rh
  // is already known
  if (data_len <3) {
    ESP_LOGE(TAG, "Received invalid calibration payload");
    return;
  }
  float cal_ppm;
  int args_parsed = sscanf(data, "%f", &cal_ppm);
  if (1 == args_parsed) {
    if (cal_ppm > 400.0 && cal_ppm <2000.) {
      mq135_info.cal_ppm = cal_ppm;
    } else {
      ESP_LOGE(TAG, "Received invalid calibration ppm");
      return;
    }
    mq135_info.cal_temperature = dht_info.temperature;
    mq135_info.cal_rel_humidity = dht_info.relative_humidity;
    mq135.calibrateRZero(mq135_info.cal_ppm, mq135_info.cal_temperature, mq135_info.cal_rel_humidity);
  } else {
    ESP_LOGE(TAG, "Received invalid calibration payload - not enough parameters");
  }
}
