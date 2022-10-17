
#include "hal/uart_types.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <esp_modbus_slave.h>
#include <string.h>

#define RS485_UART_NUM UART_NUM_2
#define BUF_SIZE (127)
#define READ_TIMEOUT 3

#define MB_REG_INPUT_START_AREA0 (0)
#define MB_REG_HOLDING_START_AREA0 (0)
#define MB_REG_HOLD_CNT (100)
#define MB_REG_INPUT_CNT (100)

#define MB_SLAVE_ADDR 1
#define MB_DEV_SPEED 9600

static const char* TAG = "MODBUS";
// mb_slave_interface_t* ;
void* modbus_handler = NULL;

mb_register_area_descriptor_t reg_area;
uint16_t holding_reg_area[MB_REG_HOLD_CNT] = { 0 };
uint16_t input_reg_area[MB_REG_INPUT_CNT] = { 0 };

// void rs485_task(void*) {
//   vTaskDelay(pdMS_TO_TICKS(1000));
//   esp_log_level_set(TAG, ESP_LOG_DEBUG);
//
//   ESP_ERROR_CHECK(mbc_slave_init(MB_PORT_SERIAL_SLAVE, &modbus_handler));
//   if (NULL == modbus_handler) {
//     ESP_LOGE(TAG, "Error allocating modbus handler");
//     vTaskDelete(NULL);
//     return;
//   }
//
//   reg_area.type = MB_PARAM_HOLDING;
//   reg_area.start_offset = MB_REG_HOLDING_START_AREA0;
//   reg_area.address = (void*)&holding_reg_area[0];
//   reg_area.size = sizeof(holding_reg_area) << 1;
//   ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));
//
//   reg_area.type = MB_PARAM_INPUT;
//   reg_area.start_offset = MB_REG_INPUT_START_AREA0;
//   reg_area.address = (void*)&input_reg_area[0];
//   reg_area.size = sizeof(input_reg_area) << 1;
//   ESP_ERROR_CHECK(mbc_slave_set_descriptor(reg_area));
//
//   mb_communication_info_t comm_info;
//   comm_info.mode = MB_MODE_RTU;
//   comm_info.slave_addr = MB_SLAVE_ADDR;
//   comm_info.port = UART_NUM_2;
//   comm_info.baudrate = MB_DEV_SPEED;
//   comm_info.parity = MB_PARITY_NONE;
//   ESP_ERROR_CHECK(mbc_slave_setup((void*)&comm_info));
//
//   static const char *test_data = "test data test data";
//   auto bytes = uart_write_bytes_with_break(UART_NUM_2, test_data, strlen(test_data), 100 );
//   ESP_LOGI(TAG, "Pushed %d bytes", bytes);
//
//   ESP_ERROR_CHECK(mbc_slave_start());
//
//   ESP_LOGI(TAG, "Started");
//   for (;;) {
//     auto events = mbc_slave_check_event((mb_event_group_t)0xF);
//     ESP_LOGI(TAG, "%x", events);
//   }
//   vTaskDelete(NULL);
// }

void rs485_task(void*) {
  vTaskDelay(pdMS_TO_TICKS(1000));
  const uart_port_t uart_num = RS485_UART_NUM;
  uart_config_t uart_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
    .source_clk = UART_SCLK_APB
  };

  esp_log_level_set(TAG, ESP_LOG_DEBUG);

  ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL,
  0)); ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(uart_num, GPIO_NUM_17, GPIO_NUM_16, -1, -1)
  );
  ESP_ERROR_CHECK(uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX));
  uint32_t* uart_rs485_conf_reg = (uint32_t*)0x3FF6E044;
  auto curr_conf_reg = *uart_rs485_conf_reg;
  *uart_rs485_conf_reg = ( curr_conf_reg | 0x10 ) & ( ~0x08 );
  ESP_ERROR_CHECK(uart_set_rx_timeout(uart_num, READ_TIMEOUT));
  // uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
  ESP_LOGI(TAG, "UART start receive loop.\r\n");

  for (;;) {
    static const char *test = "test bytes test bytes";
    uart_write_bytes_with_break(uart_num, test, strlen(test), 100);
    // int length = 0;
    // ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t*)&length));
    // if (length>0){
    //   length = uart_read_bytes(uart_num, data, length, 100);
    //   ESP_LOGI(TAG, "Received %d bytes\n", length);
    //   ESP_LOGI(TAG, "%s\n", data);
    //   uart_flush(uart_num);
    // }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  vTaskDelete(NULL);
}
