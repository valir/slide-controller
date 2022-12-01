
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <stdlib.h>

#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include <simpleDSTadjust.h>

static const char* TAG = "WIFI";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

struct dstRule startRule = { "EEST", Last, Sun, Mar, 2, 3600 };
struct dstRule endRule = { "EET", Last, Sun, Oct, 2, 0 };
simpleDSTadjust dstAdjusted(startRule, endRule);

TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t eventsTaskHandle = NULL;
void mqttTask(void*);
void eventsTask(void*);
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT
      && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 10) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGI(TAG, "connect to the AP fail");
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void wifiTask(void*)
{
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t* netif = esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(esp_netif_set_hostname(netif, "wc-" CONFIG_HOSTNAME));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  wifi_config_t wifi_config;
  // not using aggregate initializer because of this
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=55227
  strncpy((char*)wifi_config.sta.ssid, CONFIG_WIFI_SSID,
      sizeof(wifi_config.sta.ssid));
  strncpy((char*)wifi_config.sta.password, CONFIG_WIFI_PASS,
      sizeof(wifi_config.sta.password));
  wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
  wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_LOGI(TAG, "wifi_init_sta finished.");

  for (bool isSane = true; isSane;) {
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi started");

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    while (isSane) {
      /* Waiting until either the connection is established
       * (WIFI_CONNECTED_BIT) or connection failed for the maximum number of
       * re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see
       * above) */
      EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE,
          portMAX_DELAY);

      if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_WIFI_SSID,
            CONFIG_WIFI_PASS);
        if (NULL == eventsTaskHandle) {
          xTaskCreatePinnedToCore(
              eventsTask, "eventsTask", 8192, NULL, 1, &eventsTaskHandle, 1);
        }
        if (NULL == mqttTaskHandle) {
          xTaskCreatePinnedToCore(
              mqttTask, "mqttTask", 8192, NULL, 1, &mqttTaskHandle, 0);
        }
      } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
            CONFIG_WIFI_SSID, CONFIG_WIFI_PASS);
        ESP_ERROR_CHECK(esp_wifi_stop());
        ESP_LOGI(TAG, "wifi stopped");
      } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        isSane = false;
        break; // exit task - this will require device reset
      }
    }
  }
  /* The event will not be processed after unregister */
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
  vEventGroupDelete(s_wifi_event_group);
  vTaskDelete(NULL);
}
