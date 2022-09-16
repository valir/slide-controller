#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "bsec2.h"
#include "bsec_datatypes.h"
#include "display.h"
#include "events.h"
#include "sensors.h"

#define PIN_NUM_SDI 26
#define PIN_NUM_CLK 25
#define PIN_NUM_CS 33
#define I2C_MASTER_NUM 0
#define I2C_ADDR 0x77 // BME680 address is hardcoded

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const char* TAG = "SENSORS";

extern TaskHandle_t displayTaskHandle;

Sensors_Info sensors_info;

BME68X_INTF_RET_TYPE bme_i2c_read(
    uint8_t reg_adr, uint8_t* reg_data, uint32_t length, void*)
{
  esp_err_t res = i2c_master_write_read_device(I2C_MASTER_NUM, I2C_ADDR,
      &reg_adr, 1, reg_data, length, pdMS_TO_TICKS(1000));
  return (res == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

BME68X_INTF_RET_TYPE bme_i2c_write(
    uint8_t reg_adr, const uint8_t* reg_data, uint32_t length, void*)
{
  const uint32_t DATA_LEN = length + 1;
  uint8_t data[DATA_LEN];
  data[0] = reg_adr;
  memcpy(&data[1], reg_data, length);
  esp_err_t res = i2c_master_write_to_device(
      I2C_MASTER_NUM, I2C_ADDR, data, DATA_LEN, pdMS_TO_TICKS(1000));
  return (res == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
}

void bme_delay_us(uint32_t period, void*)
{
  ESP_LOGD(TAG, "delay: %d", period);
  vTaskDelay(pdMS_TO_TICKS(period / 1000));
}

// struct bme68x_dev bme = {
//   .intf_ptr = nullptr,
//   .intf = BME68X_I2C_INTF,
//   .read = bme_i2c_read,
//   .write = bme_i2c_write,
//   .delay_us = bme_delay_us,
// };
// void bme680_init()
// {
//   int8_t res = bme68x_init(&bme);
//   if (BME68X_OK != res) {
//     ESP_LOGE(TAG, "BME680: init failure");
//   } else {
//     ESP_LOGI(TAG, "BME680: init successful");
//   }
// }
//
// bme68x_conf conf = {
//   .os_hum = BME68X_OS_16X,
//   .os_temp = BME68X_OS_2X,
//   .os_pres = BME68X_OS_1X,
//   .filter = BME68X_FILTER_OFF,
//   .odr = BME68X_ODR_NONE,
// };
// bme68x_heatr_conf heat_conf = {
//   .enable = BME68X_ENABLE,
//   .heatr_temp = 300,
//   .heatr_dur = 100,
// };
// void initiate_measurement()
// {
//   auto res = bme68x_set_conf(&conf, &bme);
//   if (BME68X_OK != res) {
//     ESP_LOGE(TAG, "BME680: bme68x_set_conf failed");
//     return;
//   } else {
//     ESP_LOGI(TAG, "BME680: configured");
//   }
//
//   res = bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heat_conf, &bme);
//   if (BME68X_OK != res) {
//     ESP_LOGE(TAG, "BME680: bme68x_set_heatr_conf failed");
//     return;
//   } else {
//     ESP_LOGI(TAG, "BME680: heater configured");
//   }
//
//   res = bme68x_set_op_mode(BME68X_FORCED_MODE, &bme);
//   if (BME68X_OK != res) {
//     ESP_LOGE(TAG, "BME680: bme68x_set_op_mode failed");
//   } else {
//     ESP_LOGI(TAG, "BME680: measurement started");
//   }
// }
//
// void read_raw_sensors() {
//   auto del_period = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &bme) *
//   (heat_conf.heatr_dur * 1000); bme.delay_us(del_period, bme.intf_ptr);
//   bme68x_data data;
//   uint8_t n_fields;
//   auto res = bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &bme);
//   if (BME68X_OK != res) {
//     ESP_LOGE(TAG, "BME680: bme68x_get_data failed");
//   } else {
//     ESP_LOGI(TAG, "BME680: we've got data");
//   }
//   if (n_fields) {
//     ESP_LOGI(TAG, "BME680: T %.2f, P %.2f, RH %.2f, GAS_R %.2f, STAT
//     0x%x\n",
//                    data.temperature,
//                    data.pressure,
//                    data.humidity,
//                    data.gas_resistance,
//                    data.status);
//   }
// }
//
// void compute_sensors()
// {
//   // using BSec library
// }
//
// static void sensors_periodic_read(void*)
// {
//   initiate_measurement();
//   read_raw_sensors();
//   compute_sensors();
// }
//
static void sensors_state_backup(void*)
{
  // TODO
}

static void restore_sensors_state()
{
  // TODO
}

Bsec2 bsec2;

void sensors_data_callback(
    const bme68xData data, const bsecOutputs outputs, const Bsec2 bsec)
{
  ESP_LOGI(TAG,
      "BME680: sensor data: T %.2f, P %.2f, RH %.2f, GAS_R %.2f, STAT 0x%x",
      data.temperature, data.pressure, data.humidity, data.gas_resistance,
      data.status);
  for (int i = 0; i < outputs.nOutputs; i++) {
    const bsecData& bsdata = outputs.output[i];
    ESP_LOGD(TAG, "BSec: %d = %f", bsdata.sensor_id, bsdata.signal);
    switch (bsdata.sensor_id) {
    case BSEC_OUTPUT_STABILIZATION_STATUS:
      ESP_LOGI(TAG, "BSec: stabilization status %.1f", bsdata.signal);
      break;
    case BSEC_OUTPUT_RUN_IN_STATUS:
      ESP_LOGI(TAG, "BSec: run in status %.1f", bsdata.signal);
      sensors_info.status = bsdata.signal == 1.0 ? SENSOR_STATUS_RUNNING
                                                 : SENSOR_STATUS_STABILIZING;
      events.postGasStatusEvent(bsdata.signal == 1.0 ? true : false);
      break;
    case BSEC_OUTPUT_IAQ:
      ESP_LOGI(TAG, "BSec: IAQ %.2f", bsdata.signal);
      sensors_info.iaq = bsdata.signal + sensors_info.cal_iaq;
      events.postIAQEvent(sensors_info.iaq);
      break;
    case BSEC_OUTPUT_CO2_EQUIVALENT:
      ESP_LOGI(TAG, "BSec: CO2 %.2f", bsdata.signal);
      events.postAirCO2Event(bsdata.signal);
      break;
    case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
      ESP_LOGI(TAG, "BSec: VOC %.2f", bsdata.signal);
      events.postAirVOCEvent(bsdata.signal);
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
      ESP_LOGI(TAG, "BSec: TEMP %.2f", bsdata.signal);
      sensors_info.temperature = bsdata.signal + sensors_info.cal_temperature;
      events.postAirTemperatureEvent(sensors_info.temperature);
      break;
    case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
      ESP_LOGI(TAG, "BSec: RH %.2f%%", bsdata.signal);
      sensors_info.relative_humidity = bsdata.signal + sensors_info.cal_rel_humidity;
      events.postAirHumidityEvent(sensors_info.relative_humidity);
      break;
    case BSEC_OUTPUT_RAW_PRESSURE:
      ESP_LOGI(TAG, "BSec: BAR %.2f pa", bsdata.signal);
      events.postAirPressureEvent(bsdata.signal/100.0); // use hPa
      break;
    }
  }
  xTaskNotify(displayTaskHandle, DISPLAY_UPDATE_WIDGETS, eSetBits);
}

void bme680_init()
{
  bsec2.attachCallback(sensors_data_callback);
  if (!bsec2.begin(BME68X_I2C_INTF, bme_i2c_read, bme_i2c_write, bme_delay_us,
          nullptr)) {
    ESP_LOGE(TAG, "Cannot bsec2.begin");
  }
  bsecSensor sensor_list[] = {
    BSEC_OUTPUT_STABILIZATION_STATUS,
    BSEC_OUTPUT_RUN_IN_STATUS,
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_CO2_EQUIVALENT,
    BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_RAW_PRESSURE,
  };
  if (!bsec2.updateSubscription(
          sensor_list, sizeof(sensor_list) / sizeof(sensor_list[0]))) {
    ESP_LOGE(TAG, "Cannot bsec2.updateSubscription");
  }
}

static void sensors_periodic_read(void*)
{
  if (!bsec2.run()) {
    ESP_LOGE(TAG, "BME680 run method failed");
  }
}

void sensors_task(void*)
{
  // vTaskDelay(pdMS_TO_TICKS(1 * 1000)); // wait for other initializations to
  //                                      // take place

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

  bme680_init();

  // retrieve any previously saved state from MQTT before proceeding
  // restore_sensors_state();

  ESP_LOGI(TAG, "Starting sensors periodic read");
  esp_timer_create_args_t timer_args = { .callback = sensors_periodic_read,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1 };
  esp_timer_handle_t timer_handle;
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, 5 * 1000 * 1000));

  ESP_LOGI(TAG, "Starting sensors state periodic backup");
  esp_timer_create_args_t backup_timer_args
      = { .callback = sensors_state_backup,
          .arg = nullptr,
          .dispatch_method = ESP_TIMER_TASK,
          .skip_unhandled_events = 1 };
  esp_timer_handle_t backup_timer_handle;
  ESP_ERROR_CHECK(esp_timer_create(&backup_timer_args, &backup_timer_handle));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(backup_timer_handle, 3600 * 1000 * 1000ul));

  vTaskDelete(nullptr);
}
