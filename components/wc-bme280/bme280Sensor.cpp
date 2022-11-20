#include "bme280Sensor.h"
#include <bme280.h>
#include <algorithm>
#include "sensors.h"
#include "events.h"


#define PIN_NUM_SDI 26
#define PIN_NUM_CLK 25
#define I2C_MASTER_NUM 0
#define I2C_ADDR 0x77 // BME680 address is hardcoded

static const char* TAG = "BME280";

uint8_t dev_addr = BME280_I2C_ADDR_PRIM;
static struct bme280_data comp_data;
static struct bme280_dev dev;
bool sensor_init_ok = false;

void bme280Sensor::init()
{
  dev.intf_ptr = &dev_addr;
  dev.intf = BME280_I2C_INTF;
  dev.read = i2c_read;
  dev.write = i2c_write;
  dev.delay_us = delay_us;

  _res = bme280_init(&dev);
  if (_res < 0) {
    ESP_LOGE(TAG, "_init error %d, halting", _res);
    return;
  }

  /* Recommended mode of operation: Indoor navigation */
  dev.settings.osr_h = BME280_OVERSAMPLING_1X;
  dev.settings.osr_p = BME280_OVERSAMPLING_16X;
  dev.settings.osr_t = BME280_OVERSAMPLING_2X;
  dev.settings.filter = BME280_FILTER_COEFF_16;

  uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL
      | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

  _res = bme280_set_sensor_settings(settings_sel, &dev);
  if (_res < 0) {
    ESP_LOGE(TAG, "_set_sensor_settings status %d", _res);
    return;
  }

  /*Calculate the minimum delay required between consecutive measurement based
   * upon the sensor enabled and the oversampling configuration. */
  _req_delay = bme280_cal_meas_delay(&dev.settings);
  ESP_LOGI(TAG, "req_delay is %d", _req_delay);
  _read_delay_until
      = std::max(3u, _req_delay / 1000); // this will be handled in sensors_timer

  _res = bme280_set_sensor_mode(BME280_FORCED_MODE, &dev);
  if (_res < 0) {
    ESP_LOGE(TAG, "_set_sensor_mode status %d", _res);
    return;
  }
}

void bme280Sensor::sensors_timer()
{
  if (_res < 0)
    return;
  if (_read_delay_until-- > 0)
    return;
  _read_delay_until = std::max(3u, _req_delay / 1000);

  _res = bme280_get_sensor_data(BME280_ALL, &comp_data, &dev);
  if (_res <0){
    ESP_LOGE(TAG, "_get_sensor_data status %d", _res);
    return;
  }
  sensors_info.set_status(  SENSOR_STATUS_RUNNING );
  sensors_info.set_temperature( comp_data.temperature );
  sensors_info.set_rel_humidity( comp_data.humidity );
  sensors_info.set_pressure( comp_data.pressure );

  // initiate next measurement
  _res = bme280_set_sensor_mode(BME280_FORCED_MODE, &dev);
  if (_res < 0) {
    ESP_LOGE(TAG, "_set_sensor_mode status %d", _res);
    return;
  }
}
