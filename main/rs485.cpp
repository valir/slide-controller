
#include "hal/uart_types.h"
#include <driver/uart.h>
#include <esp_log.h>
#include <driver/gpio.h>


#define RS485_UART_NUM UART_NUM_2
#define TAG "RS485"
#define BUF_SIZE ( 127 )
#define READ_TIMEOUT 3

void rs485_task(void*) {
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

  ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0));
  ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(uart_num, GPIO_NUM_17, GPIO_NUM_16, -1, -1) );
  ESP_ERROR_CHECK(uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX));
  ESP_ERROR_CHECK(uart_set_rx_timeout(uart_num, READ_TIMEOUT));
  uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
  ESP_LOGI(TAG, "UART start receive loop.\r\n");

  for (;;) {
    int length = 0;
    ESP_ERROR_CHECK(uart_get_buffered_data_len(uart_num, (size_t*)&length));
    if (length>0){
      length = uart_read_bytes(uart_num, data, length, 100);
      ESP_LOGI(TAG, "Received %d bytes\n", length);
      ESP_LOGI(TAG, "%s\n", data);
      uart_flush(uart_num);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  vTaskDelete(NULL);
}
