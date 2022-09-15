#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2c.h>
#include <driver/gpio.h>
#include <cstring>


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

class BME680 {
  public:
    BME680() {}
    void init();
    void initiate_measurement();
    bool wait_measurement_end();
    float get_temperature();
    float get_humidity();
    float get_pressure();
  private:
    bool _gas_valid;
    enum Register : uint8_t {
      REG_STATUS = 0x73,
      REG_RESET = 0xE0,
      REG_ID = 0xD0,
      REG_CONFIG = 0x75,
      REG_CTRL_MEAS = 0x74,
      REG_CTRL_HUM = 0x72,
      REG_CTRL_GAS_1 = 0x71,
      REG_CTRL_GAS_0 = 0x70,
      REG_GAS_WAIT_9 = 0x6D,
      REG_GAS_WAIT_8 = 0x6C,
      REG_GAS_WAIT_7 = 0x6B,
      REG_GAS_WAIT_6 = 0x6A,
      REG_GAS_WAIT_5 = 0x69,
      REG_GAS_WAIT_4 = 0x68,
      REG_GAS_WAIT_3 = 0x67,
      REG_GAS_WAIT_2 = 0x66,
      REG_GAS_WAIT_1 = 0x65,
      REG_GAS_WAIT_0 = 0x64,
      REG_RES_HEAT_9 = 0x63,
      REG_RES_HEAT_8 = 0x62,
      REG_RES_HEAT_7 = 0x61,
      REG_RES_HEAT_6 = 0x60,
      REG_RES_HEAT_5 = 0x5F,
      REG_RES_HEAT_4 = 0x5E,
      REG_RES_HEAT_3 = 0x5D,
      REG_RES_HEAT_2 = 0x5C,
      REG_RES_HEAT_1 = 0x5B,
      REG_RES_HEAT_0 = 0x5A,
      REG_IDAC_HEAT_9 = 0x59,
      REG_IDAC_HEAT_8 = 0x58,
      REG_IDAC_HEAT_7 = 0x57,
      REG_IDAC_HEAT_6 = 0x56,
      REG_IDAC_HEAT_5 = 0x55,
      REG_IDAC_HEAT_4 = 0x54,
      REG_IDAC_HEAT_3 = 0x53,
      REG_IDAC_HEAT_2 = 0x52,
      REG_IDAC_HEAT_1 = 0x51,
      REG_IDAC_HEAT_0 = 0x50,
      REG_GAS_R_LSB = 0x2B,
      REG_GAS_R_MSB = 0x2A,
      REG_HUM_LSB = 0x26,
      REG_HUM_MSB = 0x25,
      REG_TEMP_XLSB = 0x24,
      REG_TEMP_LSB = 0x23,
      REG_TEMP_MSB = 0x22,
      REG_PRESS_XLSB = 0x21,
      REG_PRESS_LSB = 0x20,
      REG_PRESS_MSB = 0x1F,
      REG_EAS_STATUS_0 = 0x1D,
      REG_PAR_G1 = 0xED,
      REG_PAR_G2_LSB = 0xEB,
      REG_PAR_G2_MSB = 0xEC,
      REG_PAR_G3 = 0xEE,
      REG_RES_HEAT_RANGE = 0x02,
      REG_RES_HEAT_VAL = 0x00,
    };

    template <Register R> struct RegDescriptor {
        static inline constexpr uint8_t _register = R;
    };
    template <Register R> struct ByteRegDescriptor : public RegDescriptor<R> {
      typedef uint8_t Data;
    };
    template <Register R> struct WordRegDescriptor : public RegDescriptor<R> {
      typedef struct {
        uint8_t lsb;
        uint8_t msb;
        uint8_t* operator & () { return &lsb; }
      } Data;
    };
    template <Register R> struct ThreeByteRegDescriptor : public RegDescriptor<R> {
      typedef struct {
        uint8_t msb;
        uint8_t lsb;
        uint8_t xlsb;
        uint8_t* operator & () { return &msb; }
      } Data;
    };
    struct REG_ID_Descriptor : public ByteRegDescriptor<REG_ID> {};
    struct REG_CTRL_MEAS_Descriptor : public ByteRegDescriptor<REG_CTRL_MEAS> { };
    struct REG_CTRL_HUM_Descriptor : public ByteRegDescriptor<REG_CTRL_HUM> {};
    struct REG_PAR_G1_Descriptor : public ByteRegDescriptor<REG_PAR_G1> {};
    struct REG_PAR_G2_Descriptor : public WordRegDescriptor<REG_PAR_G2_LSB> {};
    struct REG_PAR_G3_Descriptor : public ByteRegDescriptor<REG_PAR_G3> {};
    struct REG_RES_HEAT_RANGE_Descriptor : public ByteRegDescriptor<REG_RES_HEAT_RANGE> {};
    struct REG_RES_HEAT_VAL_Descriptor : public ByteRegDescriptor<REG_RES_HEAT_VAL> {};
    struct REG_GAS_WAIT_0_Descriptor : public ByteRegDescriptor<REG_GAS_WAIT_0> {};
    struct REG_RES_HEAT_0_Descriptor : public ByteRegDescriptor<REG_RES_HEAT_0> {};
    struct REG_CTRL_GAS_1_Descriptor : public ByteRegDescriptor<REG_CTRL_GAS_1> {};
    struct REG_CTRL_GAS_0_Descriptor : public ByteRegDescriptor<REG_CTRL_GAS_0> {};
    struct REG_EAS_STATUS_0_Descriptor : public ByteRegDescriptor<REG_EAS_STATUS_0> {};
    struct REG_GAS_R_LSB_Descriptor : public ByteRegDescriptor<REG_GAS_R_LSB> {};
    struct REG_TEMP_MSB_Descriptor : public ThreeByteRegDescriptor<REG_TEMP_MSB> {};


    template <class R>
    class ReadTransaction {
      public:
      ReadTransaction(){}
      typename R::Data execute() {
        typename R::Data result;
        ESP_LOGD(TAG, "BME680 read reg 0x%x", R::_register);
        esp_err_t res = i2c_master_write_read_device(I2C_MASTER_NUM, I2C_ADDR,
            &R::_register, 1,
            &result, sizeof(result), pdMS_TO_TICKS(1000));
        ESP_ERROR_CHECK(res);
        return result;
      }
    };
    template <class R>
    class WriteTransaction {
      public:
        WriteTransaction(){}
        void execute(typename R::Data data) {
          constexpr auto DATA_LEN = sizeof(typename R::Data) +1;
          uint8_t buffer[DATA_LEN];
          buffer[0] = R::_register;
          uint8_t* pd = static_cast<uint8_t*>(buffer);
          for (auto i=1; i<DATA_LEN; i++, pd++) {
            buffer[i] = *pd;
          }
          esp_err_t res = i2c_master_write_to_device(I2C_MASTER_NUM, I2C_ADDR,
            buffer, DATA_LEN,
            pdMS_TO_TICKS(1000));
          ESP_ERROR_CHECK_WITHOUT_ABORT(res);
        }
    };
    void select_oversampling();
    void select_iir_filter();
    void select_gas_params();
    void start_measure();
};

void BME680::init() {
  ReadTransaction<REG_ID_Descriptor> read_id;
  auto reg_id = read_id.execute();
  ESP_LOGI(TAG, "BME680 id = 0x%x", reg_id);
  if (reg_id != 0x61) {
    ESP_LOGE(TAG, "BME680 id was not the expected 0x61, sensor data is no reliable");
  }
  // ReadTransaction read_temp(REG_TEMP_MSB);
  // auto reg_temp = read_temp.execute();
  // ESP_LOGI(TAG, "BME680 temp reg = %d:%d", reg_temp.msb, reg_temp.lsb);
}

BME680 bme680;

void BME680::select_oversampling() {
  ReadTransaction<REG_CTRL_HUM_Descriptor> osrs_hum_read;
  auto osrs_hum = osrs_hum_read.execute();
  ReadTransaction<REG_CTRL_MEAS_Descriptor> osrs_read;
  auto osrs = osrs_read.execute();

  uint8_t osrs_h = 0b001;
  uint8_t desired_osrs_hum = (osrs_hum & 0xF8) | (osrs_h);
  uint8_t osrs_t, osrs_p, mode;
  osrs_t = 0b010;
  osrs_p = 0b101; // 16x oversampling
  mode = 0;
  uint8_t desired_osrs = (osrs_t << 5) | (osrs_p << 2) | mode;

  if (desired_osrs_hum != osrs_hum) {
    WriteTransaction<REG_CTRL_HUM_Descriptor> osrs_hum_write;
    osrs_hum_write.execute(desired_osrs_hum);
  }
  if (desired_osrs != osrs) {
    WriteTransaction<REG_CTRL_MEAS_Descriptor> osrs_write;
    osrs_write.execute(desired_osrs);
  }
}

void BME680::select_iir_filter() {
}

void BME680::select_gas_params() {
  WriteTransaction<REG_GAS_WAIT_0_Descriptor> gas_wait_write;
  gas_wait_write.execute(0x59);

  ReadTransaction<REG_PAR_G1_Descriptor> read_g1;
  ReadTransaction<REG_PAR_G2_Descriptor> read_g2;
  ReadTransaction<REG_PAR_G3_Descriptor> read_g3;
  ReadTransaction<REG_RES_HEAT_RANGE_Descriptor> read_res_heat_range;
  ReadTransaction<REG_RES_HEAT_VAL_Descriptor> read_res_heat_val;
  WriteTransaction<REG_RES_HEAT_0_Descriptor> heat_write;
  WriteTransaction<REG_CTRL_GAS_1_Descriptor> gas_ctrl_write;

  auto g1 = read_g1.execute();
  auto g2_raw = read_g2.execute();
  ESP_LOGI(TAG, "BME680: g2(lsb, msb) %d,%d", g2_raw.lsb, g2_raw.msb);
  uint16_t g2 = (g2_raw.msb << 8) + g2_raw.lsb;
  auto g3 = read_g3.execute();
  ESP_LOGI(TAG, "BME680: g1 = %d, g2 = %d, g3 = %d", g1, g2, g3);
  auto res_heat_range = ( read_res_heat_range.execute() & 0x30 ) >> 4;
  int8_t res_heat_val = (int8_t)read_res_heat_val.execute();
  ESP_LOGI(TAG, "BME680: heat_range = %d, heat_val = %d", res_heat_range, res_heat_val);

  constexpr double TARGET_TEMP = 300.0;
  auto var1 = ((double)g1/16.0) + 49.0;
  auto var2 = (((double)g2/32768.0) * 0.0005) + 0.00235;
  auto var3 = (double)g3 / 1024.0;
  auto var4 = var1 * (1.0 + (var2 * (double)TARGET_TEMP));
  auto var5 = var4 + (var3 * (double)23.0); // TODO - put here the real
                                            // ambient temperature
  auto res_heat_0 = (uint8_t)(3.4 * ((var5 * (4.0 / (4.0 + (double)res_heat_range)) * (1.0/(1.0 + ((double)res_heat_val * 0.002))))- 25));
  ESP_LOGI(TAG, "BME680: res_heat_0 <- %d", res_heat_0);
  heat_write.execute(res_heat_0);

  gas_ctrl_write.execute(0x0); // nbconv = 0x0, do not yet run
  gas_ctrl_write.execute(0x10); // activate gas run
}

void BME680::start_measure() {
  ReadTransaction<REG_CTRL_MEAS_Descriptor> read_mode;
  auto current_mode = read_mode.execute();
  ESP_LOGI(TAG, "BME680: current mode 0x%X", current_mode);

  WriteTransaction<REG_CTRL_MEAS_Descriptor> write_mode;
  write_mode.execute(current_mode | 0b01);
  ESP_LOGI(TAG, "BME680: measuring started");
}

void BME680::initiate_measurement() {
  select_oversampling();
  select_iir_filter();
  select_gas_params();
  start_measure();
}

bool BME680::wait_measurement_end() {
  ReadTransaction<REG_EAS_STATUS_0_Descriptor> read_status;
  auto status = read_status.execute();
  uint8_t iterations =0;
  while ( ((0x80 & status) == 0) && (iterations++ < 5)) {
    vTaskDelay(pdMS_TO_TICKS(1000)); // let the sensor do it's thing
    status = read_status.execute();
  }
  if ((status & 0x80) == 0) {
    ESP_LOGE(TAG, "BME680 sensor still on after 5s");
    return false;
  }
  ReadTransaction<REG_GAS_R_LSB_Descriptor> read_gas_status;
  auto gas_status = read_gas_status.execute();
  if ((0x20 & gas_status) == 0) {
    ESP_LOGE(TAG, "BME680: gas measurement was unsuccessful 0x%x", gas_status);
    _gas_valid = false;
  }
  if ((0x10 & gas_status) == 0) {
    ESP_LOGE(TAG, "BME680: gas temperature was not stable");
    _gas_valid = false;
  } else {
    _gas_valid = true;
  }
  ESP_LOGI(TAG, "BME680 - data ready %d", iterations);
  return true;
}

float BME680::get_temperature() {
  ReadTransaction<REG_TEMP_MSB_Descriptor> read_temp;
  auto t = read_temp.execute();
  ESP_LOGI(TAG, "BME680: t %d %d %d", t.msb , t.lsb, t.xlsb >> 4);
  return 0.0;
}

float BME680::get_humidity() {
  return 0.0;
}

float BME680::get_pressure() {
  return 0.0;
}

void read_raw_sensors() {
  auto temp = bme680.get_temperature();
  auto rh = bme680.get_humidity();
  auto p = bme680.  get_pressure();
  ESP_LOGI(TAG, "BME680: T %f | RH %f%% | P %f", temp, rh, p);
}

void compute_sensors() {
  // using BSec library
}

static void sensors_periodic_read(void*) {
  bme680.initiate_measurement();
  if ( bme680.wait_measurement_end() ) {
    read_raw_sensors();
    compute_sensors();
  }
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

  bme680.init();

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

