
#include "sensors.h"
#include <cstdio>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <stdio.h>
#include <string.h>

#include <events.h>

static const char* TAG = "MQTT";
#define MQTT_TOPIC_NIGHT_MODE "cmd/barlog/night_mode"
#define MQTT_TOPIC_LIGHTS "cmd/" ENV_HOSTNAME "/lights"
#define MQTT_TOPIC_POLL_NIGHT_MODE "state/barlog/night_mode"
#define MQTT_TOPIC_CALIBRATE "cmd/" ENV_HOSTNAME "/calibrate"
#define MQTT_TOPIC_OTA "cmd/" ENV_HOSTNAME "/ota"

struct MqttSubscription {
  const char* TOPIC;
  int qos;
  int subscription_id;
  typedef void handler_func(const char*, int);
  handler_func* func;
  void subscribe(esp_mqtt_client_handle_t client)
  {
    subscription_id = esp_mqtt_client_subscribe(client, TOPIC, qos);
    ESP_LOGI(TAG, "Subscribed to %s with %u", TOPIC, subscription_id);
  }
  void operator()(const char* data, int data_len)
  {
    if (func == nullptr) {
      ESP_LOGE(TAG, "Unhandled topic subscription %s received data %.*s",
          TOPIC, data_len, data);
    } else {
      (*func)(data, data_len);
    }
  };
};

void setDisplayBacklight(bool on);
bool night_mode = false;
void handleNightMode(const char* data, int data_len)
{
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
  ESP_LOGE(
      TAG, "handleNightMode received unknown parameter %.*s", data_len, data);
}

void handleCalibrate(const char* data, int data_len)
{
  // calibrate message payload has three float values: temp RH IAQ
  if (data == nullptr || data_len < 5) {
    ESP_LOGE(TAG, "handleCalibrate: Incorrect data received");
    return;
  }
  if (3
      != sscanf(data, "%f %f %f", &sensors_info.cal_temperature,
          &sensors_info.cal_rel_humidity, &sensors_info.cal_iaq)) {
    ESP_LOGE(TAG, "handleCalibrate: incorrect params %.*s", data_len, data);
  } else {
    ESP_LOGI(TAG, "handleCalibrate: accepted %.*s", data_len, data);
  }
}

void handleOtaCmd(const char* data, int data_len)
{
  if (strncasecmp(data, "start", data_len)) { }
  if (strncasecmp(data, "status", data_len)) { }
  if (strncasecmp(data, "reboot", data_len)) { }
}

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
struct MqttSubscription mqttSubscriptions[]
    = { { .TOPIC = MQTT_TOPIC_NIGHT_MODE, .qos = 0, .func = handleNightMode },
        { .TOPIC = MQTT_TOPIC_LIGHTS, .qos = 1 },
        { .TOPIC = MQTT_TOPIC_CALIBRATE, .qos = 0, .func = handleCalibrate },
        { .TOPIC = MQTT_TOPIC_OTA, .qos = 1, .func = handleOtaCmd },
        { .TOPIC = nullptr } };

static const char* CONFIG_BROKER_URL = "mqtt://bb-master";
#ifdef ENV_HOSTNAME
#define WALL_CONTROLLER_NAME ENV_HOSTNAME
#else
#error Please define ENV_HOSTNAME
#endif
#define MQTT_PREFIX "barlog/" WALL_CONTROLLER_NAME

static void log_error_if_nonzero(const char* message, int error_code)
{
  if (error_code != 0) {
    ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
  }
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t base,
    int32_t event_id, void* event_data)
{
  ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base,
      event_id);
  esp_mqtt_event_handle_t event
      = reinterpret_cast<esp_mqtt_event_handle_t>(event_data);
  esp_mqtt_client_handle_t client = event->client;
  int msg_id;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    {
      MqttSubscription* subscription = &mqttSubscriptions[0];
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
      MqttSubscription* subscription = &mqttSubscriptions[0];
      while (subscription->TOPIC) {
        if (strncmp(subscription->TOPIC, event->topic, event->topic_len)
            == 0) {
          char* buffer = (char*)malloc(event->data_len + 1);
          if (buffer) {
            memcpy(buffer, event->data, event->data_len);
            buffer[event->data_len] = 0;
            (*subscription)(buffer, event->data_len);
            free(buffer);
          } else {
            ESP_LOGE(TAG, "Cannot allocate data buffer");
          }
        }
        subscription++;
      }
    }
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      log_error_if_nonzero(
          "reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
      log_error_if_nonzero(
          "reported from tls stack", event->error_handle->esp_tls_stack_err);
      log_error_if_nonzero("captured as transport's socket errno",
          event->error_handle->esp_transport_sock_errno);
      ESP_LOGI(TAG, "Last errno string (%s)",
          strerror(event->error_handle->esp_transport_sock_errno));
    }
    break;
  default:
    ESP_LOGI(TAG, "Other event id:%d", event->event_id);
    break;
  }
}

void heartbeat(void*)
{
  static bool just_started = true;
  if (just_started) {
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
  events.postHeartbeatEvent();
}

void mqttTask(void*)
{
  esp_mqtt_client_config_t mqtt_cfg = {
    .uri = CONFIG_BROKER_URL,
  };
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client,
      (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  ESP_ERROR_CHECK(esp_mqtt_client_start(client));

  esp_timer_create_args_t heartbeat_timer_args = { .callback = heartbeat,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1 };
  esp_timer_handle_t heart_beat_timer;
  ESP_ERROR_CHECK(esp_timer_create(&heartbeat_timer_args, &heart_beat_timer));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(heart_beat_timer, 3 * 1000 * 1000ul));

  for (;;) {
    MqttEventInfo mqttEvent = events.waitNextEvent();
    char topic[80];
    snprintf(topic, sizeof(topic) / sizeof(topic[0]), MQTT_PREFIX "/%s",
        mqttEvent.name);
    ESP_LOGI(TAG, "%s %s", topic, mqttEvent.data.c_str());
    esp_mqtt_client_publish(
        client, topic, mqttEvent.data.c_str(), mqttEvent.data.size(), 1, 0);
  }
  vTaskDelete(nullptr);
}
