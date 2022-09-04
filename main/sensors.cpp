#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/spi_master.h>
#include <driver/gpio.h>

#include "display.h"
#include "events.h"
#include "sensors.h"

#define PIN_NUM_MISO 19
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK 18
#define PIN_NUM_CS 33

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

static const char* TAG = "SENSORS";

float temperature_calibration = -1.5f;

extern TaskHandle_t displayTaskHandle;

Sensors_Info sensors_info;

spi_device_handle_t bme_dev;

class BME680_ReadTransaction {
  public:
    BME680_ReadTransaction(uint8_t register_address) {
      if (0x80 & register_address) {
      // TODO perform automatic page change command
      }
      _trans.tx_data[0] = 0x80 | register_address;
    }
  protected:
    void execute() {
      _trans.flags = SPI_TRANS_USE_TXDATA;
      _trans.cmd = 1;
      _trans.addr = 7;
      _trans.length = 0;
      _trans.rxlength = 16;
      ESP_ERROR_CHECK(spi_device_polling_transmit(bme_dev, &_trans));
    }
  private:
    spi_transaction_t _trans;
};

class BME680_WriteTransaction {
};

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

  ESP_LOGI(TAG, "Initializing bus SPI3");
  spi_bus_config_t buscfg = {
    .mosi_io_num = PIN_NUM_MOSI,
    .miso_io_num = PIN_NUM_MISO,
    .sclk_io_num = PIN_NUM_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .data4_io_num = -1,
    .data5_io_num = -1,
    .data6_io_num = -1,
    .data7_io_num = -1,
    .max_transfer_sz = 32
  };
  ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));

  spi_device_interface_config_t bmecfg = {
    .command_bits = 1, // command is READ or WRITE
    .address_bits = 7,
    .mode = 0,
    .clock_speed_hz = (1*1000*1000), // 1MHz even though chip supports 10Mhz
    .input_delay_ns = 40,
    .spics_io_num = PIN_NUM_CS,
    .flags = SPI_DEVICE_HALFDUPLEX,
    .queue_size = 1,
  };
  ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &bmecfg, &bme_dev));

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
  ESP_ERROR_CHECK(esp_timer_start_periodic(backup_timer_handle, 3600*1000*1000));

  vTaskDelete(nullptr);
}

