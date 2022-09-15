#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c.h>
#include <driver/gpio.h>

#include "display.h"
#include "events.h"
#include "sensors.h"

#define PIN_NUM_SDI 26
#define PIN_NUM_CLK 25
#define PIN_NUM_CS  33
#define I2C_MASTER_NUM 0
#define I2C_ADDR 0x77 // BME680 address is hardcoded

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const char* TAG = "SENSORS";

float temperature_calibration = -1.5f;

extern TaskHandle_t displayTaskHandle;

Sensors_Info sensors_info;

void bme680_init() {
}

void select_oversampling() {
}

void select_iir_filter() {
}

void select_gas_params() {
}

void start_measure() {
}

void initiate_measurement() {
  select_oversampling();
  select_iir_filter();
  select_gas_params();
  start_measure();
}

void read_raw_sensors() {
}

void compute_sensors() {
  // using BSec library
}

static void sensors_periodic_read(void*) {
  initiate_measurement();
  read_raw_sensors();
  compute_sensors();
}

static void sensors_state_backup(void*) {
  // TODO
}

static void restore_sensors_state() {
  // TODO
}

void sensors_task(void*)
{
  vTaskDelay(pdMS_TO_TICKS(1*1000)); // wait for other initializations to take
                                     // place

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
  ESP_ERROR_CHECK(i2c_driver_install( I2C_MASTER_NUM, i2c_conf.mode, 0, 0, 0 ));

  bme680_init();

  // retrieve any previously saved state from MQTT before proceeding
  restore_sensors_state();

  ESP_LOGI(TAG, "Starting sensors periodic read");
  esp_timer_create_args_t timer_args = { .callback = sensors_periodic_read,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1 };
  esp_timer_handle_t timer_handle;
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, 10*1000*1000));

  ESP_LOGI(TAG, "Starting sensors state periodic backup");
  esp_timer_create_args_t backup_timer_args = { .callback = sensors_state_backup,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1 };
  esp_timer_handle_t backup_timer_handle;
  ESP_ERROR_CHECK(esp_timer_create(&backup_timer_args, &backup_timer_handle));
  ESP_ERROR_CHECK(esp_timer_start_periodic(backup_timer_handle, 3600*1000*1000ul));

  vTaskDelete(nullptr);
}

