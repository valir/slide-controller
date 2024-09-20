
#include "mqtt.h"
#include "backlight.h"
#include "buzzer.h"
#include "events.h"
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
extern bool wifi_connected;
void otaTask(void*);

static const char* TAG = "MQTT";
extern TaskHandle_t mqttTaskHandle;
#define MQTT_TOPIC_NIGHT_MODE "cmd/barlog/night_mode"
#define MQTT_TOPIC_LIGHTS "cmd/" CONFIG_HOSTNAME "/lights"
#define MQTT_TOPIC_POLL_NIGHT_MODE "state/barlog/night_mode"
#define MQTT_TOPIC_CALIBRATE "cmd/" CONFIG_HOSTNAME "/calibrate"
#define MQTT_TOPIC_EXT_CALIBRATE "cmd/" CONFIG_HOSTNAME "/ext_calibrate"
#define MQTT_TOPIC_OTA "cmd/" CONFIG_HOSTNAME "/ota"
#define MQTT_TOPIC_SOUND "cmd/" CONFIG_HOSTNAME "/sound"
#define MQTT_TOPIC_DISPLAY "cmd/" CONFIG_HOSTNAME "/display"
#define MQTT_TOPIC_CONTROL "cmd/" CONFIG_HOSTNAME "/control"
#define MQTT_TOPIC_AIR_QUALITY "state/" CONFIG_HOSTNAME "/air_quality"

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
void handleAirQuality(const char* data, int data_len)
{
  // air quality message payload has two float values: CO2 IAQ
  if (data == nullptr || data_len < 3) {
    ESP_LOGE(TAG, "handleAirQuality: Incorrect data received");
    return;
  }
  float co2, iaq;
  if (2 != sscanf(data, "%f %f", &co2, &iaq)) {
    ESP_LOGE(TAG, "handleAirQuality: incorrect params %.*s", data_len, data);
  } else {
    ESP_LOGI(TAG, "handleAirQuality: accepted %.*s", data_len, data);
    events.postAirCO2Event(co2);
    events.postIAQEvent(iaq);
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
  float cal_temperature, cal_rel_humidity;
  if (2 != sscanf(data, "%f %f", &cal_temperature, &cal_rel_humidity)) {
    ESP_LOGE(
        TAG, "handleExtCalibrate: incorrect params %.*s", data_len, data);
  } else {
    ESP_LOGI(TAG, "handleExtCalibrate: accepted %.*s", data_len, data);
    sensors_info.set_ext_cal_values(cal_temperature, cal_rel_humidity);
    buzzer.ackTone();
  }
}
#endif

void handleOtaCmd(const char* data, int data_len)
{
  constexpr size_t CMD_LENGTH = 8;
  const char* STR_CMD_START = "start";
  char cmd[CMD_LENGTH + 1];
  sscanf(data, "%7s", cmd);
  if (strncasecmp(cmd, STR_CMD_START, CMD_LENGTH) == 0) {
    xTaskCreate(&otaTask, "otaTask", 8192, NULL, 3, &otaTaskHandle);
  }
}

void handleSound(const char* data, int data_len)
{
  ESP_LOGI(TAG, "handleSound %*s", data_len, data);
  if (strncasecmp(data, "alert on", 8) == 0) {
    BackLight::turnOn();
    buzzer.startAlert();
  } else if (strncasecmp(data, "alert off", 9) == 0) {
    buzzer.stopAlert();
  } else if (strncasecmp(data, "doorbell", 8) == 0) {
    buzzer.doorbell();
  } else if (strncasecmp(data, "warning on", 10) == 0) {
    BackLight::turnOn();
    buzzer.startWarning();
  } else if (strncasecmp(data, "warning off", 11) == 0) {
    buzzer.stopWarning();
  }
}

// handle the display mqtt event; when payload is "on" or "off" turn the
// display on or off. However, if the display is turned on, then a timer will
// be set to turn it off after a while.
void handleDisplay(const char* data, int data_len)
{
  if (strncasecmp(data, "on", data_len) == 0) {
    BackLight::turnOn();
    return;
  }
  if (strncasecmp(data, "off", data_len) == 0) {
    BackLight::turnOff();
    return;
  }
  ESP_LOGE(TAG, "handleDisplay received unknown parameter %.*s", data_len, data);
}

void handleControl(const char* data, int data_len)
{
  if (strncasecmp(data, "reboot", data_len) == 0) {
    ESP_LOGI(TAG, "Rebooting");
    esp_restart();
  }
}

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
struct MqttSubscription mqttSubscriptions[] = {
  { .TOPIC = MQTT_TOPIC_OTA, .qos = 1, .func = handleOtaCmd },
  { .TOPIC = MQTT_TOPIC_NIGHT_MODE, .qos = 0, .func = handleNightMode },
  { .TOPIC = MQTT_TOPIC_LIGHTS, .qos = 1 },
  { .TOPIC = MQTT_TOPIC_SOUND, .qos = 1, .func = handleSound },
#ifdef CONFIG_HAS_INTERNAL_SENSOR
  { .TOPIC = MQTT_TOPIC_CALIBRATE, .qos = 0, .func = handleCalibrate },
#ifdef CONFIG_USE_SENSOR_BME280
  { .TOPIC = MQTT_TOPIC_AIR_QUALITY, .qos = 1, .func = handleAirQuality },
#endif
#endif
#if CONFIG_HAS_EXTERNAL_SENSOR
  { .TOPIC = MQTT_TOPIC_EXT_CALIBRATE, .qos = 1, .func = handleExtCalibrate },
#endif
  { .TOPIC = MQTT_TOPIC_DISPLAY, .qos = 1, .func = handleDisplay },
  { .TOPIC = MQTT_TOPIC_CONTROL, .qos = 1, .func = handleControl},
  { .TOPIC = nullptr }
};

static const char* CONFIG_BROKER_URL = "mqtt://bb-master";
#define MQTT_PREFIX "barlog/" CONFIG_HOSTNAME
static esp_mqtt_client_handle_t client;
bool mqtt_connected = false;

#ifndef CONFIG_HAS_INTERNAL_SENSOR
static esp_timer_handle_t heartbeat_timer;
static void postHeartbeatEvent(void*) { events.postHeartbeatEvent(); }
#endif

static bool needSubscribe = true;
static void onMqttConnectedEvent(esp_mqtt_client_handle_t client)
{
  if (needSubscribe) {
    MqttSubscription* subscription = &mqttSubscriptions[0];
    while (subscription->TOPIC) {
      subscription->subscribe(client);
      subscription++;
    }
    needSubscribe = false;
  }
#ifndef CONFIG_HAS_INTERNAL_SENSOR
  esp_timer_create_args_t timer_args = { .callback = postHeartbeatEvent,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .skip_unhandled_events = 1 };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &heartbeat_timer));
  ESP_ERROR_CHECK(
      esp_timer_start_periodic(heartbeat_timer, 60 * 1000 * 1000));
#endif
}

static void onMqttDisconnectedEvent()
{
  mqtt_connected = false;
  xTaskNotify(mqttTaskHandle, 0x1, eSetBits);
#ifndef CONFIG_HAS_INTERNAL_SENSOR
  esp_timer_stop(heartbeat_timer);
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
    ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
    mqtt_connected = true;
    onMqttConnectedEvent(client);
    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
    onMqttDisconnectedEvent();
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
  int retain = 0;
  memset(data, 0, DATA_BUSIZE);
  switch (event.event) {
  case EVENT_HEARTBEAT:
    eventName = "heartbeat";
    strcpy(data, "on");
    retain = 1;
    break;
  case EVENT_SENSOR_TEMPERATURE:
    eventName = "air_temperature";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_temperature);
    retain = 1;
    break;
  case EVENT_SENSOR_HUMIDITY:
    eventName = "air_humidity";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_humidity);
    retain = 1;
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
    retain = 1;
    break;
#if CONFIG_USE_SENSOR_BME680
  case EVENT_SENSOR_IAQ:
    eventName = "air_iaq";
    snprintf(data, DATA_BUSIZE, "%.2f", event.air_iaq);
    retain = 1;
    break;
  case EVENT_SENSOR_CO2:
    eventName = "air_co2";
    snprintf(data, DATA_BUSIZE, "%.2f", event.air_co2);
    retain = 1;
    break;
  case EVENT_SENSOR_VOC:
    eventName = "air_voc";
    snprintf(data, DATA_BUSIZE, "%.2f", event.air_voc);
    retain = 1;
    break;
#endif
  case EVENT_SENSOR_PRESSURE:
    eventName = "air_pressure";
    snprintf(data, DATA_BUSIZE, "%.0f", event.air_pressure);
    retain = 1;
    break;
#if CONFIG_HAS_EXTERNAL_SENSOR
  case EVENT_SENSOR_EXT_TEMPERATURE:
    eventName = "ext_temperature";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_temperature);
    retain = 1;
    break;
  case EVENT_SENSOR_EXT_HUMIDITY:
    eventName = "ext_humidity";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_humidity);
    retain = 1;
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
    return;
  }

  if (!mqtt_connected) {
    ESP_LOGE(TAG, "ignoring event %s", eventName);
  } else {
    // finally, post the event
    const char* eventData = constData ? constData : data;
    static char* topic = NULL;
    constexpr size_t TOPIC_LEN = 80;
    if (topic == NULL) {
      topic = (char*)malloc(TOPIC_LEN);
    }
    snprintf(topic, TOPIC_LEN, MQTT_PREFIX "/%s", eventName);
    ESP_LOGI(TAG, "%s %s", topic, eventData);
    esp_mqtt_client_enqueue(
        client, topic, eventData, strlen(eventData), 1, retain, false);
  }
}

void mqttTask(void* h)
{
  ESP_LOGI(TAG, "Starting up");
  esp_mqtt_client_config_t mqtt_cfg = {
    .uri = CONFIG_BROKER_URL,
    .lwt_topic = MQTT_PREFIX "/heartbeat",
    .lwt_msg = "offline",
    .lwt_qos = 1,
    .lwt_retain = 1
  };

  needSubscribe = true;
  static MqttEventObserver observer;
  events.registerObserver(&observer);

  client = esp_mqtt_client_init(&mqtt_cfg);
  esp_mqtt_client_register_event(client,
      (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  esp_mqtt_client_start(client);

  uint32_t bits;
  while (
      pdTRUE == xTaskNotifyWait(ULONG_MAX, ULONG_MAX, &bits, portMAX_DELAY)) {
    if (!wifi_connected) {
      break; // terminate this task so it will be recreated when wifi is back
    }
  }

  ESP_LOGI(TAG, "Terminating task.");
  esp_mqtt_client_stop(client);
  esp_mqtt_client_destroy(client);
  events.unregisterObserver(&observer);
  mqttTaskHandle = NULL;
  vTaskDelete(nullptr);
}
