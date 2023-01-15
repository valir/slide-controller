
#include "mqtt.h"
#include "buzzer.h"
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
#if defined(CONFIG_HAS_INTERNAL_SENSOR) || defined(CONFIG_HAS_EXTERNAL_SENSOR)
#include "sensors.h"
#endif
#include <events.h>

extern TaskHandle_t otaTaskHandle;
void otaTask(void*);

static const char* TAG = "MQTT";
#define MQTT_TOPIC_NIGHT_MODE "cmd/barlog/night_mode"
#define MQTT_TOPIC_LIGHTS "cmd/" CONFIG_HOSTNAME "/lights"
#define MQTT_TOPIC_POLL_NIGHT_MODE "state/barlog/night_mode"
#define MQTT_TOPIC_CALIBRATE "cmd/" CONFIG_HOSTNAME "/calibrate"
#define MQTT_TOPIC_EXT_CALIBRATE "cmd/" CONFIG_HOSTNAME "/ext_calibrate"
#define MQTT_TOPIC_OTA "cmd/" CONFIG_HOSTNAME "/ota"
#define MQTT_TOPIC_SOUND "cmd/" CONFIG_HOSTNAME "/sound"

class MqttEventObserver : public EventObserver {
  virtual void notice(const Event&) override;
  virtual const char* name() override { return "mqtt"; }
};

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

#if CONFIG_HAS_INTERNAL_SENSOR
void handleCalibrate(const char* data, int data_len)
{
  // calibrate message payload has three float values: temp RH IAQ
  if (data == nullptr || data_len < 5) {
    ESP_LOGE(TAG, "handleCalibrate: Incorrect data received");
    return;
  }
  float cal_temperature, cal_rel_humidity, cal_iaq;
  if (3
      != sscanf(
          data, "%f %f %f", &cal_temperature, &cal_rel_humidity, &cal_iaq)) {
    ESP_LOGE(TAG, "handleCalibrate: incorrect params %.*s", data_len, data);
  } else {
    ESP_LOGI(TAG, "handleCalibrate: accepted %.*s", data_len, data);
    sensors_info.set_cal_values(cal_temperature, cal_rel_humidity, cal_iaq);
  }
}
#endif

#if CONFIG_HAS_EXTERNAL_SENSOR
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
#endif

void handleOtaCmd(const char* data, int data_len)
{
  constexpr size_t CMD_LENGTH = 8;
  const char* STR_CMD_START = "start";
  char cmd[CMD_LENGTH + 1];
  sscanf(data, "%8s", cmd);
  if (strncasecmp(cmd, STR_CMD_START, CMD_LENGTH) == 0) {
    xTaskCreate(&otaTask, "otaTask", 8192, NULL, 3, &otaTaskHandle);
  }
}

void handleSound(const char* data, int data_len)
{
  if (strncasecmp(data, "alert on", 8) == 0) {
    buzzer.startAlert();
  } else if (strncasecmp(data, "alert off", 9) == 0) {
    buzzer.stopAlert();
  } else if (strncasecmp(data, "doorbell", 8) == 0) {
    buzzer.doorbell();
  }
}

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
struct MqttSubscription mqttSubscriptions[] = {
  { .TOPIC = MQTT_TOPIC_NIGHT_MODE, .qos = 0, .func = handleNightMode },
  { .TOPIC = MQTT_TOPIC_LIGHTS, .qos = 1 },
  { .TOPIC = MQTT_TOPIC_SOUND, .qos = 1, .func = handleSound },
#ifdef CONFIG_HAS_INTERNAL_SENSOR
  { .TOPIC = MQTT_TOPIC_CALIBRATE, .qos = 0, .func = handleCalibrate },
#endif
  { .TOPIC = MQTT_TOPIC_OTA, .qos = 1, .func = handleOtaCmd },
#if CONFIG_HAS_EXTERNAL_SENSOR
  { .TOPIC = MQTT_TOPIC_EXT_CALIBRATE, .qos = 1, .func = handleExtCalibrate },
#endif
  { .TOPIC = nullptr }
};

static const char* CONFIG_BROKER_URL = "mqtt://bb-master";
#define MQTT_PREFIX "barlog/" CONFIG_HOSTNAME
static esp_mqtt_client_handle_t client;

#ifndef CONFIG_HAS_INTERNAL_SENSOR
static void postHeartbeatEvent(void*) { events.postHeartbeatEvent(); }
#endif

static void onMqttConnectedEvent(esp_mqtt_client_handle_t client)
{
  MqttSubscription* subscription = &mqttSubscriptions[0];
  while (subscription->TOPIC) {
    subscription->subscribe(client);
    subscription++;
  }
#ifndef CONFIG_HAS_INTERNAL_SENSOR
  esp_timer_create_args_t timer_args = { .callback = postHeartbeatEvent,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1 };
  esp_timer_handle_t timer_handle;
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, 10 * 1000 * 1000));
#endif
}

bool mqtt_connected = false;

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
    mqtt_connected = true;
    onMqttConnectedEvent(client);
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
    mqtt_connected = false;
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
    break;
  default:
    ESP_LOGD(TAG, "Other event id:%d", event->event_id);
    break;
  }
}

void MqttEventObserver::notice(const Event& event)
{
  const char* eventName = NULL;
  constexpr size_t DATA_BUSIZE = 8;
  char data[DATA_BUSIZE];
  const char* constData = NULL;
  memset(data, 0, DATA_BUSIZE);
  switch (event.event) {
  case EVENT_HEARTBEAT:
    eventName = "heartbeat";
    strcpy(data, "on");
    break;
  case EVENT_SENSOR_TEMPERATURE:
    eventName = "air_temperature";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_temperature);
    break;
  case EVENT_SENSOR_HUMIDITY:
    eventName = "air_humidity";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_humidity);
    break;
  case EVENT_SCREEN_TOUCHED: {
    eventName = "touched";
    const char* gestures[TOUCH_GESTURE_MAX] = {
      "invalid",
      "short",
      "long",
      "swipe-left",
      "swipe-right",
      "swipe-up",
      "swipe-down",
    };
    constData = gestures[event.touch_info.gesture];
  } break;
  case EVENT_SENSOR_GAS_STATUS: // BSEC_OUTPUT_RUN_IN_STATUS
    eventName = "gas_status";
    snprintf(data, DATA_BUSIZE, "%d", event.gas_status);
    break;
  case EVENT_SENSOR_IAQ:
    eventName = "air_iaq";
    snprintf(data, DATA_BUSIZE, "%.2f", event.air_iaq);
    break;
  case EVENT_SENSOR_CO2:
    eventName = "air_co2";
    snprintf(data, DATA_BUSIZE, "%.2f", event.air_co2);
    break;
  case EVENT_SENSOR_VOC:
    eventName = "air_voc";
    snprintf(data, DATA_BUSIZE, "%.2f", event.air_voc);
    break;
  case EVENT_SENSOR_PRESSURE:
    eventName = "air_pressure";
    snprintf(data, DATA_BUSIZE, "%.0f", event.air_pressure);
    break;
#if ENV_EXT_SENSOR == 1
  case EVENT_SENSOR_EXT_TEMPERATURE:
    eventName = "ext_temperature";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_temperature);
    break;
  case EVENT_SENSOR_EXT_HUMIDITY:
    eventName = "ext_humidity";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_humidity);
    break;
#endif
  case EVENT_OTA_STARTED:
    eventName = "ota";
    strcpy(data, "start");
    break;
  case EVENT_OTA_DONE_OK:
    eventName = "ota";
    strcpy(data, "OK");
    break;
  case EVENT_OTA_DONE_FAIL:
    eventName = "ota";
    strcpy(data, "FAIL");
    break;
  default:
    ESP_LOGE(TAG, "Unknown event type %d", event.event);
  }

  if (!mqtt_connected) {
    ESP_LOGE(TAG, "ignoring event %s", eventName);
  } else {
    // finally, post the event
    const char* eventData = constData ? constData : data;
    static char* topic;
    constexpr size_t TOPIC_LEN = 80;
    topic = (char*)malloc(TOPIC_LEN);
    snprintf(topic, TOPIC_LEN, MQTT_PREFIX "/%s", eventName);
    ESP_LOGI(TAG, "%s %s", topic, eventData);
    esp_mqtt_client_publish(
        client, topic, eventData, strlen(eventData), 1, 0);
  }
}

void mqttTask(void*)
{
  esp_mqtt_client_config_t mqtt_cfg = {
    .uri = CONFIG_BROKER_URL,
  };
  client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client,
      (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  ESP_ERROR_CHECK(esp_mqtt_client_start(client));

  static MqttEventObserver observer;
  events.registerObserver(&observer);

  while (true) {
    vTaskDelay(pdMS_TO_TICKS(10000));
    if (!mqtt_connected) {
      esp_mqtt_client_reconnect(client);
    }
  }

  vTaskDelete(nullptr);
}
