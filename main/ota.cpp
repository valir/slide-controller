#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

TaskHandle_t otaTaskHandle;
static const char* TAG = "OTA";

const esp_partition_t* part;
esp_ota_handle_t ota_handle;

esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
  static uint32_t ota_offset = 0;
  switch (evt->event_id) {
  case HTTP_EVENT_ERROR:
    ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
    break;
  case HTTP_EVENT_ON_CONNECTED:
    ESP_ERROR_CHECK(esp_ota_begin(part, OTA_SIZE_UNKNOWN, &ota_handle));
    ota_offset = 0;
    ESP_LOGI(TAG, "Started downloading");
    break;
  case HTTP_EVENT_HEADER_SENT:
    ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
    break;
  case HTTP_EVENT_ON_HEADER:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
        evt->header_value);
    break;
  case HTTP_EVENT_ON_DATA:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
    esp_ota_write_with_offset(
        ota_handle, evt->data, evt->data_len, ota_offset);
    ota_offset += evt->data_len;
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED:
    ESP_ERROR_CHECK(esp_ota_end(ota_handle));
    ESP_LOGI(TAG, "Finished downloading %d bytes", ota_offset);
    break;
  }
  return ESP_OK;
}

void otaTask(void* param)
{
  ESP_LOGI(TAG, "start");

  part = esp_ota_get_next_update_partition(esp_ota_get_running_partition());
  ESP_LOGI(TAG, "Writing to partition %s", part->label);
  ESP_ERROR_CHECK(esp_partition_erase_range(part, 0, part->size));

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  esp_http_client_config_t http_config
      = { .url = "http://" ENV_OTA_IP "/wc_ota/image-" ENV_HOSTNAME ".bin",
          .event_handler = http_event_handler };
  esp_http_client_handle_t http_client = esp_http_client_init(&http_config);
  ESP_ERROR_CHECK(esp_http_client_perform(http_client));
  ESP_ERROR_CHECK(esp_http_client_cleanup(http_client));

  constexpr size_t OTA_CHECKSUM_SIZE = 32;
  uint8_t ota_checksum[OTA_CHECKSUM_SIZE] = { 0 };
  ESP_ERROR_CHECK(esp_partition_get_sha256(part, ota_checksum));
  char hash_print[OTA_CHECKSUM_SIZE * 2 + 1];
  for (int i = 0; i < OTA_CHECKSUM_SIZE; i++) {
    sprintf(&hash_print[i * 2], "%02x", ota_checksum[i]);
  }
  char* known_sha256 = (char*)param;
  if (strncmp(known_sha256, hash_print, OTA_CHECKSUM_SIZE * 2) == 0) {
    ESP_LOGI(TAG,
        "Succesffully downloaded OTA image, activating new application...");
    ESP_ERROR_CHECK(esp_ota_set_boot_partition(part));
    esp_restart();
  } else {
    ESP_LOGE(
        TAG, "Expected checksum %s, actual %s", known_sha256, hash_print);
  }

  free(param);

  vTaskDelete(NULL);
}
