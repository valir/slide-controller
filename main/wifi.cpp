
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <WiFi.h>
#include <simpleDSTadjust.h>

#ifndef ENV_WIFI_SSID
#error Please define ENV_WIFI_SSID
#endif
#ifndef ENV_WIFI_PASS
#error Please define ENV_WIFI_PASS
#endif
#ifndef ENV_HOSTNAME
#error Please define ENV_HOSTNAME
#endif

static const char* TAG = "WIFI";
static const char* WIFI_SSID = ENV_WIFI_SSID;
static const char* WIFI_PASS = ENV_WIFI_PASS;
static const char* WIFI_HOSTNAME = ENV_HOSTNAME;

void connectWifi()
{
  if (WiFi.status() == WL_CONNECTED)
    return;
  ESP_LOGI(TAG, "Connnecting to WiFi %s / %s", WIFI_SSID, WIFI_PASS);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  auto wifiStatus = WiFi.status();
  ESP_LOGI(TAG, "Status changed to %d", wifiStatus);
  while (wifiStatus != WL_CONNECTED) {
    auto newStatus = WiFi.status();
    if (newStatus != wifiStatus) {
      ESP_LOGI(TAG, "Status changed to %d", newStatus);
      wifiStatus = newStatus;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  ESP_LOGI(TAG, "Connected to WiFi '%s'", WIFI_SSID);
  // if we got up following an OTA request, then validate this version
  const esp_partition_t* running_part = esp_ota_get_running_partition();
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running_part, &ota_state) == ESP_OK) {
    if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
      ESP_LOGI(TAG, "Successfully cancelled OTA rollback");
    } else {
      ESP_LOGE(TAG, "Failed to cancel OTA rollback");
    }
  }
}

enum WifiTaskState {
  WIFI_STATE_STARTING,
  WIFI_STATE_UPDATE_DATA,
  WIFI_STATE_RUNNING,
  WIFI_STATE_RECONNECTING
};

#define NTP_SERVERS "cerberus.rusu.info"
struct dstRule startRule = { "EEST", Last, Sun, Mar, 2, 3600 };
struct dstRule endRule = { "EEST", Last, Sun, Oct, 2, 0 };
simpleDSTadjust dstAdjusted(startRule, endRule);

TaskHandle_t mqttTaskHandle = NULL;
void mqttTask(void*);

void wifiTask(void*)
{
  vTaskDelay(pdMS_TO_TICKS(5000));
  WifiTaskState state = WIFI_STATE_STARTING;
  for (;;) {
    switch (state) {
    case WIFI_STATE_STARTING: {
      connectWifi();
      if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI(TAG, "Updating time...");
        configTime(2 * 3600, 0, NTP_SERVERS);
        while (!time(nullptr))
          vTaskDelay(pdMS_TO_TICKS(100));
        char* dstAbbrev;
        time_t now = dstAdjusted.time(&dstAbbrev);
        ESP_LOGI(TAG, "got time: %s", ctime(&now));
        state = WIFI_STATE_UPDATE_DATA;
      } else {
        state = WIFI_STATE_RECONNECTING;
      }
    } break;
    case WIFI_STATE_UPDATE_DATA: {
      char* dstAbbrev;
      time_t now = dstAdjusted.time(&dstAbbrev);
      ESP_LOGI(TAG, "got time: %s", ctime(&now));
      state = WIFI_STATE_RUNNING;
    } break;
    case WIFI_STATE_RUNNING:
      if (NULL == mqttTaskHandle) {
        xTaskCreatePinnedToCore(
            mqttTask, "mqttTask", 8192, NULL, 1, &mqttTaskHandle, 0);
      }
      if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGI(TAG, "Connection lost, reconnecting");
        state = WIFI_STATE_STARTING;
        vTaskDelete(mqttTaskHandle);
        mqttTaskHandle = NULL;
      }
      break;
    case WIFI_STATE_RECONNECTING:
      connectWifi();
      if (WiFi.status() == WL_CONNECTED) {
        state = WIFI_STATE_RUNNING;
      } else {
        state = WIFI_STATE_RECONNECTING;
      }
      break;
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
  vTaskDelete(NULL);
}
