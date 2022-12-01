
#include <esp_err.h>
#include <esp_l.h>
#include <esp_system.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_modbus_slave.h>


#pragma pack(push,1)
struct CoilsParams {
};
#pragma pack(pop)

#pragma pack(push,1)
struct InputRegParams {
  uint8_t status = 0;
  float air_temperature = 0.0f;
  float air_humidity = 0.0f;
  float air_quality = 0.0f;
  uint8_t wifi_strength = 0;
};
#pragma pack(pop)

#pragma pack(push,1)
struct HoldingRegParam {
  float requested_temperature =0;
};
#pragma pack(pop)

static const char* TAG = "MODBUS";

void modbusTask(void*) {
  mb_register_area_descriptor_t reg_area;

  void* slave_handler =nullptr;
  ESP_ERROR_CHECK(mbc_slave_init_tcp(&slave_handler));

  mb_communication_info_t comm_info = { 0 };
  comm_info.ip_addr_type = MB_IPV4;
  comm_info.ip_mode = MB_MODE_TCP;
  comm_info.ip_port = 502;
  comm_info.ip_addr = nullptr;
  comm_info.ip_netif_ptr = WiFi.channel();
}
