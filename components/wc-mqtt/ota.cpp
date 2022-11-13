#include "buzzer.h"
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "events.h"


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
    buzzer.playOtaDownloading();
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
    // buzzer.playOtaDownloading();
    break;
  case HTTP_EVENT_ON_FINISH:
    ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
    break;
  case HTTP_EVENT_DISCONNECTED: {
    ESP_LOGI(TAG, "Finished downloading %d bytes", ota_offset);
    auto err = esp_ota_end(ota_handle);
    if (ESP_OK == err) {
      ESP_LOGI(TAG,
          "Succesffully downloaded OTA image, activating new application...");
      ESP_ERROR_CHECK(esp_ota_set_boot_partition(part));
      events.postOtaDoneOk();
      vTaskDelay(pdMS_TO_TICKS(500)); // allow for MQTT event to go out
      esp_restart();
    } else if (ESP_ERR_OTA_VALIDATE_FAILED) {
      ESP_LOGE(TAG, "Image validation failed");
      buzzer.playOtaFailed();
      events.postOtaDoneFail();
    }
  } break;
  }
  return ESP_OK;
}

void otaTask(void*)
{
  ESP_LOGI(TAG, "start");
  buzzer.playOtaStart();
  events.postOtaStarted();

  part = esp_ota_get_next_update_partition(esp_ota_get_running_partition());
  ESP_LOGI(TAG, "Writing to partition %s", part->label);
  ESP_ERROR_CHECK(esp_partition_erase_range(part, 0, part->size));

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
  esp_http_client_config_t http_config
      = { .url = "http://" CONFIG_OTA_IP "/wc_ota/image-" CONFIG_HOSTNAME ".bin",
          .event_handler = http_event_handler };
  esp_http_client_handle_t http_client = esp_http_client_init(&http_config);
  ESP_ERROR_CHECK(esp_http_client_perform(http_client));
  ESP_ERROR_CHECK(esp_http_client_cleanup(http_client));

  vTaskDelete(NULL);
}
