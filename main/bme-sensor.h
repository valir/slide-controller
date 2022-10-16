
#pragma once
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <cstring>

#define I2C_MASTER_NUM 0
#define I2C_ADDR 0x77 // BME680 address is hardcoded

template <typename RET_TYPE, RET_TYPE RET_OK, RET_TYPE RET_FAIL>
class bmeSensor {
  protected:
  static RET_TYPE i2c_read(
      uint8_t reg_adr, uint8_t* reg_data, uint32_t length, void*)
  {
    esp_err_t res = i2c_master_write_read_device(I2C_MASTER_NUM, I2C_ADDR,
        &reg_adr, 1, reg_data, length, pdMS_TO_TICKS(1000));
    return (res == ESP_OK) ? RET_OK : RET_FAIL;
  }

  static RET_TYPE i2c_write(
      uint8_t reg_adr, const uint8_t* reg_data, uint32_t length, void*)
  {
    const uint32_t DATA_LEN = length + 1;
    uint8_t data[DATA_LEN];
    data[0] = reg_adr;
    memcpy(&data[1], reg_data, length);
    esp_err_t res = i2c_master_write_to_device(
        I2C_MASTER_NUM, I2C_ADDR, data, DATA_LEN, pdMS_TO_TICKS(1000));
    return (res == ESP_OK) ? RET_OK : RET_FAIL;
  }

  static void delay_us(uint32_t period, void*)
  {
    ESP_LOGD("BME", "delay: %d", period);
    vTaskDelay(pdMS_TO_TICKS(period / 1000));
  }
};
