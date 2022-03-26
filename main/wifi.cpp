
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include <esp_log.h>

#include <WiFi.h>
#include <simpleDSTadjust.h>

static const char* TAG = "WIFI";
static const char* WIFI_SSID = "workers";
static const char* WIFI_PASS = "Ahboo4ooYei0Gaikahma";
static const char* WIFI_HOSTNAME = "dormitor";

void connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return;
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
}

enum WifiTaskState {
  WIFI_STATE_STARTING,
  WIFI_STATE_UPDATE_DATA,
  WIFI_STATE_RUNNING,
  WIFI_STATE_RECONNECTING
};

#define NTP_SERVERS "cerberus.rusu.info"
struct dstRule startRule = {"EEST", Last, Sun, Mar, 2, 3600};
struct dstRule endRule = {"EEST", Last, Sun, Oct, 2, 0};
simpleDSTadjust dstAdjusted(startRule, endRule);

TaskHandle_t mqttTaskHandle = NULL;
void mqttTask(void*);

void wifiTask(void*) {
  vTaskDelay(pdMS_TO_TICKS(5000));
  WifiTaskState state = WIFI_STATE_STARTING;
  for (;;){
    switch (state) {
      case WIFI_STATE_STARTING:
        {
          connectWifi();
          if (WiFi.status() == WL_CONNECTED) {
            ESP_LOGI(TAG, "Updating time...");
            configTime(2 * 3600, 0, NTP_SERVERS);
            while (!time(nullptr)) vTaskDelay(pdMS_TO_TICKS(100));
            char *dstAbbrev;
            time_t now = dstAdjusted.time(&dstAbbrev);
            ESP_LOGI(TAG, "got time: %s", ctime(&now));
            state = WIFI_STATE_UPDATE_DATA;
          } else {
            state = WIFI_STATE_RECONNECTING;
          }
        }
        break;
      case WIFI_STATE_UPDATE_DATA:
        {
          char *dstAbbrev;
          time_t now = dstAdjusted.time(&dstAbbrev);
          ESP_LOGI(TAG, "got time: %s", ctime(&now));
          state = WIFI_STATE_RUNNING;
        }
        break;
      case WIFI_STATE_RUNNING:
        if (NULL == mqttTaskHandle) {
          xTaskCreatePinnedToCore(mqttTask, "mqttTask", 8192, NULL, 1, &mqttTaskHandle, 0);
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
