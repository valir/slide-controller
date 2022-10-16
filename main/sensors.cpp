#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "bme280-sensor.h"
#include "bme680-sensor.h"
#include "display.h"
#include "events.h"
#include "sensors.h"

#define PIN_NUM_SDI 26
#define PIN_NUM_CLK 25
#define I2C_MASTER_NUM 0
#define I2C_ADDR 0x77 // BME680 address is hardcoded

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const char* TAG = "SENSORS";

Sensors_Info sensors_info;

void heartbeat() { events.postHeartbeatEvent(); }

template <class S> class sensor_wrapper {
  static S _sensor;

  public:
  sensor_wrapper() { _sensor.init(); }
  static void sensors_timer(void*)
  {
    static uint8_t heartbeat_counter = 0;
    if ((heartbeat_counter++ % 3) == 0) {
      heartbeat();
    }

    static bool measuring = false;

    if (measuring) {
      // bsec2.setTemperatureOffset(sensors_info.cal_temperature);
      _sensor.sensors_timer();
    } else {
      // activate measuring only after 10 heartbeats have been sent
      // so homeassistant get a change to send calibration data first
      measuring = heartbeat_counter > 30;
    }
  }
};

template <class S> S sensor_wrapper<S>::_sensor;

void sensors_task(void*)
{
  vTaskDelay(pdMS_TO_TICKS(1 * 30000)); // wait for other initializations to
                                        // take place

  ESP_LOGI(TAG, "Initializing I2C");
  i2c_config_t i2c_conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = PIN_NUM_SDI,
    .scl_io_num = PIN_NUM_CLK,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
  };
  i2c_conf.master.clk_speed = 100000;
  ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &i2c_conf));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0));

  sensor_wrapper< ENV_SENSOR_TYPE > sensor;

  // retrieve any previously saved state from MQTT before proceeding
  // restore_sensors_state();

  ESP_LOGI(TAG, "Starting sensors periodic read");
  esp_timer_create_args_t timer_args = { .callback = sensor.sensors_timer,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1 };
  esp_timer_handle_t timer_handle;
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, 1 * 1000 * 1000));

  // ESP_LOGI(TAG, "Starting sensors state periodic backup");
  // esp_timer_create_args_t backup_timer_args
  //     = { .callback = sensors_state_backup,
  //         .arg = nullptr,
  //         .dispatch_method = ESP_TIMER_TASK,
  //         .skip_unhandled_events = 1 };
  // esp_timer_handle_t backup_timer_handle;
  // ESP_ERROR_CHECK(esp_timer_create(&backup_timer_args,
  // &backup_timer_handle)); ESP_ERROR_CHECK(
  //     esp_timer_start_periodic(backup_timer_handle, 3600 * 1000 * 1000ul));

  vTaskDelete(nullptr);
}
