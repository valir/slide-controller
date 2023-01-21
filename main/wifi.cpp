
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
#include <cstring>
#include <esp_sntp.h>
#include <time.h>

#include "lwip/err.h"
#include "lwip/sys.h"

static const char* TAG = "WIFI";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

TaskHandle_t mqttTaskHandle = NULL;
TaskHandle_t eventsTaskHandle = NULL;
void mqttTask(void*);
void eventsTask(void*);
static int s_retry_num = 0;
bool wifi_connected = false;

static void event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    ESP_LOGI(TAG, "EVENT_STA_START");
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT
      && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (s_retry_num < 10) {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    } else {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
      ESP_LOGI(TAG, "connect to the AP fail");
    }
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

void time_sync_notification_cb(struct timeval* tv)
{
  auto tm = localtime(&tv->tv_sec);
  ESP_LOGI(TAG, "time_sync_notificationi %s", asctime(tm));
}

void wifiTask(void*)
{
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t* netif = esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(esp_netif_set_hostname(netif, "sc-" CONFIG_HOSTNAME));

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
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
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_LOGI(TAG, "wifi_init_sta finished.");

  for (bool isSane = true; isSane;) {

    /* Waiting until either the connection is established
     * (WIFI_CONNECTED_BIT) or connection failed for the maximum number of
     * re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see
     * above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
      ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_WIFI_SSID,
          CONFIG_WIFI_PASS);

      static bool need_sntp_init = true;
      if (need_sntp_init) {
        const char* TZ_VAR = "EET-2EEST,M3.5.0,M10.5.0";
        setenv("TZ", TZ_VAR, 1);
        tzset();
        sntp_setoperatingmode(SNTP_OPMODE_POLL);
        sntp_setservername(0, CONFIG_NTP_SERVER);
        sntp_set_time_sync_notification_cb(time_sync_notification_cb);
        sntp_init();
        need_sntp_init = false;
        while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
          ESP_LOGI(TAG, "Waiting for system time to be set...");
          vTaskDelay(pdMS_TO_TICKS(2000));
        }
      }

      ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
      if (NULL == eventsTaskHandle) {
        xTaskCreatePinnedToCore(
            eventsTask, "eventsTask", 8192, NULL, 1, &eventsTaskHandle, 1);
      }
      if (NULL == mqttTaskHandle) {
        xTaskCreatePinnedToCore(
            mqttTask, "mqttTask", 8192, NULL, 1, &mqttTaskHandle, 0);
      }
      wifi_connected = true;
    } else if (bits & WIFI_FAIL_BIT) {
      ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
          CONFIG_WIFI_SSID, CONFIG_WIFI_PASS);
      ESP_LOGI(TAG, "wifi stopped");
      wifi_connected = false;
      vTaskDelay(pdMS_TO_TICKS(60 * 1000));
      ESP_LOGI(TAG, "Attempting new wifi start");
      s_retry_num = 0;
      ESP_ERROR_CHECK(esp_wifi_connect());
    } else {
      ESP_LOGE(TAG, "UNEXPECTED EVENT");
      isSane = false;
      wifi_connected = false;
      esp_restart();
    }
  }
  ESP_ERROR_CHECK(esp_wifi_stop());
  /* The event will not be processed after unregister */
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
  vEventGroupDelete(s_wifi_event_group);
  vTaskDelete(NULL);
}
