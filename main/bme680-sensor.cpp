#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "bme680-sensor.h"
#include "sensors.h"
#include "events.h"
#include <display.h>


#define I2C_MASTER_NUM 0
#define I2C_ADDR 0x77 // BME680 address is hardcoded

static const char *TAG = "BME680";

// static BME68X_INTF_RET_TYPE bme_i2c_read(
//     uint8_t reg_adr, uint8_t* reg_data, uint32_t length, void*)
// {
//   esp_err_t res = i2c_master_write_read_device(I2C_MASTER_NUM, I2C_ADDR,
//       &reg_adr, 1, reg_data, length, pdMS_TO_TICKS(1000));
//   return (res == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
// }
//
// static BME68X_INTF_RET_TYPE bme_i2c_write(
//     uint8_t reg_adr, const uint8_t* reg_data, uint32_t length, void*)
// {
//   const uint32_t DATA_LEN = length + 1;
//   uint8_t data[DATA_LEN];
//   data[0] = reg_adr;
//   memcpy(&data[1], reg_data, length);
//   esp_err_t res = i2c_master_write_to_device(
//       I2C_MASTER_NUM, I2C_ADDR, data, DATA_LEN, pdMS_TO_TICKS(1000));
//   return (res == ESP_OK) ? BME68X_OK : BME68X_E_COM_FAIL;
// }

void bme680Sensor::sensors_state_backup(void*)
{
  // TODO
}

void bme680Sensor::restore_sensors_state()
{
  // TODO
}

Bsec2 bsec2;

void bme680Sensor::sensors_data_callback(
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
      sensors_info.relative_humidity
          = bsdata.signal + sensors_info.cal_rel_humidity;
      events.postAirHumidityEvent(sensors_info.relative_humidity);
      break;
    case BSEC_OUTPUT_RAW_PRESSURE:
      ESP_LOGI(TAG, "BSec: BAR %.2f pa", bsdata.signal);
      events.postAirPressureEvent(bsdata.signal / 100.0); // use hPa
      break;
    }
  }
  xTaskNotify(displayTaskHandle, DISPLAY_UPDATE_WIDGETS, eSetBits);
}

void bme680Sensor::init()
{
  bsec2.attachCallback(sensors_data_callback);
  if (!bsec2.begin(BME68X_I2C_INTF, i2c_read, i2c_write, delay_us,
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
