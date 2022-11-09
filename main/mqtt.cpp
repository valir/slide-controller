
#include <cstdio>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <mqtt_client.h>
#include <stdio.h>
#include <string.h>
#ifdef ENV_NO_SENSOR
#include "display.h"
#endif
#include "buzzer.h"
#include "sensors.h"


#include <events.h>

extern TaskHandle_t otaTaskHandle;
void otaTask(void*);

static const char* TAG = "MQTT";
#define MQTT_TOPIC_NIGHT_MODE "cmd/barlog/night_mode"
#define MQTT_TOPIC_LIGHTS "cmd/" ENV_HOSTNAME "/lights"
#define MQTT_TOPIC_POLL_NIGHT_MODE "state/barlog/night_mode"
#define MQTT_TOPIC_CALIBRATE "cmd/" ENV_HOSTNAME "/calibrate"
#define MQTT_TOPIC_EXT_CALIBRATE "cmd/" ENV_HOSTNAME "/ext_calibrate"
#define MQTT_TOPIC_OTA "cmd/" ENV_HOSTNAME "/ota"
#define MQTT_TOPIC_SOUND "cmd/" ENV_HOSTNAME "/sound"

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
    // setDisplayBacklight(false);
    return;
  }
  if (strncasecmp(data, "off", data_len) == 0) {
    night_mode = false;
    // setDisplayBacklight(true);
    return;
  }
  ESP_LOGE(
      TAG, "handleNightMode received unknown parameter %.*s", data_len, data);
}

#ifndef ENV_NO_SENSOR
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
#endif // ENV_NO_SENSOR

void handleExtCalibrate(const char* data, int data_len)
{
  if (data == nullptr || data_len < 3) {
    ESP_LOGE(TAG, "handleExtCalibrate: incorrect data received");
    return;
  }
  if (2
      != sscanf(data, "%f %f", &sensors_info.cal_ext_temperature,
          &sensors_info.cal_ext_humidity)) {
    ESP_LOGE(
        TAG, "handleExtCalibrate: incorrect params %.*s", data_len, data);
  } else {
    ESP_LOGI(TAG, "handleExtCalibrate: accepted %.*s", data_len, data);
    buzzer.ackTone();
  }
}

void handleOtaCmd(const char* data, int data_len)
{
  constexpr size_t CMD_LENGTH = 8;
  const char* STR_CMD_START = "start";
  char cmd[CMD_LENGTH + 1];
  sscanf(data, "%8s", cmd);
  if (strncasecmp(cmd, STR_CMD_START, CMD_LENGTH) == 0) {
      xTaskCreate(
          &otaTask, "otaTask", 8192, NULL, 3, &otaTaskHandle);
  }
}

void handleSound(const char* data, int data_len)
{
  if (strncasecmp(data, "alert on", 8) == 0) {
    buzzer.startAlert();
  } else if (strncasecmp(data, "alert off", 9) ==0) {
    buzzer.stopAlert();
  } else if (strncasecmp(data, "doorbell", 8) ==0) {
    buzzer.doorbell();
  }
}

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
struct MqttSubscription mqttSubscriptions[] = {
  { .TOPIC = MQTT_TOPIC_NIGHT_MODE, .qos = 0, .func = handleNightMode },
  { .TOPIC = MQTT_TOPIC_LIGHTS, .qos = 1 },
  { .TOPIC = MQTT_TOPIC_SOUND, .qos =1, .func = handleSound },
#ifndef ENV_NO_SENSOR
  { .TOPIC = MQTT_TOPIC_CALIBRATE, .qos = 0, .func = handleCalibrate },
#endif
  { .TOPIC = MQTT_TOPIC_OTA, .qos = 1, .func = handleOtaCmd },
#if ENV_EXT_SENSOR == 1
  { .TOPIC = MQTT_TOPIC_EXT_CALIBRATE, .qos = 1, .func = handleExtCalibrate },
#endif
  { .TOPIC = nullptr }
};

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

static void postHeartbeatEvent(void*) {
  events.postHeartbeatEvent();
}

static void onMqttConnectedEvent(esp_mqtt_client_handle_t client)
{
  MqttSubscription* subscription = &mqttSubscriptions[0];
  while (subscription->TOPIC) {
    subscription->subscribe(client);
    subscription++;
  }
#ifdef ENV_NO_SENSOR
  esp_timer_create_args_t timer_args = { .callback = postHeartbeatEvent,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1 };
  esp_timer_handle_t timer_handle;
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, 10 * 1000 * 1000));
  xTaskNotify(displayTaskHandle, DISPLAY_UPDATE_WIDGETS, eSetBits);
#endif
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
    ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
    onMqttConnectedEvent(client);
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
    break;

  case MQTT_EVENT_SUBSCRIBED:
    ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
    ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
    break;
  case MQTT_EVENT_UNSUBSCRIBED:
    ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_PUBLISHED:
    ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
  case MQTT_EVENT_DATA:
    ESP_LOGD(TAG, "MQTT_EVENT_DATA");
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
    ESP_LOGD(TAG, "MQTT_EVENT_ERROR");
    // if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
    //   log_error_if_nonzero(
    //       "reported from esp-tls",
    //       event->error_handle->esp_tls_last_esp_err);
    //   log_error_if_nonzero(
    //       "reported from tls stack",
    //       event->error_handle->esp_tls_stack_err);
    //   log_error_if_nonzero("captured as transport's socket errno",
    //       event->error_handle->esp_transport_sock_errno);
    //   ESP_LOGD(TAG, "Last errno string (%s)",
    //       strerror(event->error_handle->esp_transport_sock_errno));
    // }
    break;
  default:
    ESP_LOGD(TAG, "Other event id:%d", event->event_id);
    break;
  }
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

  static char* topic;
  constexpr size_t TOPIC_LEN = 80;
  topic = (char*)malloc(TOPIC_LEN);
  for (;;) {
    MqttEventInfo mqttEvent = events.waitNextEvent();
    snprintf(topic, TOPIC_LEN, MQTT_PREFIX "/%s", mqttEvent.name);
    ESP_LOGI(TAG, "%s %s", topic, mqttEvent.data.c_str());
    esp_mqtt_client_publish(
        client, topic, mqttEvent.data.c_str(), mqttEvent.data.size(), 1, 0);
  }
  vTaskDelete(nullptr);
}
