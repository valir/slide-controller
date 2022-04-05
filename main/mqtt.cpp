
#include <stdio.h>
#include <string.h>
#include <cstdio>
#include <esp_event.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <mqtt_client.h>
#include "sensors.h"

#include <events.h>

static const char* TAG = "MQTT";
#define MQTT_TOPIC_NIGHT_MODE "cmd/barlog/night_mode"
#define MQTT_TOPIC_LIGHTS "cmd/dormitor/lights"
#define MQTT_TOPIC_POLL_NIGHT_MODE "state/barlog/night_mode"
#define MQTT_TOPIC_CALIBRATE_AIR_QUALITY "cmd/dormitor/calibrate_air_quality"

struct MqttSubscription {
  const char *TOPIC;
  int qos;
  int subscription_id;
  typedef void handler_func(const char*, int);
  handler_func *func;
  void subscribe(esp_mqtt_client_handle_t client) {
    subscription_id = esp_mqtt_client_subscribe(client, TOPIC, qos);
    ESP_LOGI(TAG, "Subscribed to %s with %u", TOPIC, subscription_id);
  }
  void operator()(const char* data, int data_len) {
    if (func == nullptr) {
      ESP_LOGE(TAG, "Unhandled topic subscription %s received data %.*s", TOPIC, data_len, data);
    } else {
      (*func)(data, data_len);
    }
  };
};

void setDisplayBacklight(bool on);
bool night_mode = false;
void handleNightMode(const char* data, int data_len) {
  // this receives only "on" or "off" string
  if (strncasecmp(data, "on", data_len) == 0) {
    night_mode = true;
    setDisplayBacklight(false);
    return;
  }
  if (strncasecmp(data, "off", data_len) == 0) {
    night_mode = false;
    setDisplayBacklight(true);
    return;
  }
  ESP_LOGE(TAG, "handleNightMode received unknown parameter %.*s", data_len, data);
}

void handleCalibrateAirQuality(const char *data, int data_len);

struct MqttSubscription mqttSubscriptions[] = {
  { .TOPIC = MQTT_TOPIC_NIGHT_MODE, .qos = 0, .func = handleNightMode },
  { .TOPIC = MQTT_TOPIC_LIGHTS, .qos = 1 },
  { .TOPIC = MQTT_TOPIC_CALIBRATE_AIR_QUALITY, .qos = 0, .func = handleCalibrateAirQuality },
  { .TOPIC = nullptr },
};


static const char* CONFIG_BROKER_URL = "mqtt://bb-master";
#define WALL_CONTROLLER_NAME "dormitor"
#define MQTT_PREFIX "barlog/" WALL_CONTROLLER_NAME

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = reinterpret_cast<esp_mqtt_event_handle_t>( event_data );
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        {
          MqttSubscription *subscription = &mqttSubscriptions[0];
          while (subscription->TOPIC) {
            subscription->subscribe(client);
            subscription++;
          }
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        {
          MqttSubscription *subscription = &mqttSubscriptions[0];
          while (subscription->TOPIC) {
            if (strncmp(subscription->TOPIC, event->topic, event->topic_len) == 0){
              (*subscription)(event->data, event->data_len);
            }
            subscription++;
          }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}


void mqttTask(void *) {
  esp_mqtt_client_config_t mqtt_cfg = {
    .uri = CONFIG_BROKER_URL,
  };
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  ESP_ERROR_CHECK( esp_mqtt_client_start(client) );
  for (;;) {
    MqttEventInfo mqttEvent = events.waitNextEvent();
    char topic[80];
    snprintf(topic, sizeof(topic)/sizeof(topic[0]), MQTT_PREFIX "/%s", mqttEvent.name);
    esp_mqtt_client_publish(client, topic, mqttEvent.data.c_str(), mqttEvent.data.size(), 1, 0);
  }
  vTaskDelete(nullptr);
}
