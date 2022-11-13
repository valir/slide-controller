
#include "events.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <string.h>
#include <string>

const char* TAG = "EVENTS";

Events events;

Events::Events()
{
  event_queue = xQueueCreate(10, sizeof(Event));
  if (event_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create the queue");
  }
}

Events::~Events() { vQueueDelete(event_queue); }

template <typename T>
struct ReduceEventsFrequency {
  T last_x;
  uint8_t count =0;
  ReduceEventsFrequency(T zero =0) : last_x(zero) {}
  bool skipEvent(T x) {
    if (last_x !=x || count++ >6) {
      count = 0;
      last_x = x;
      return false;
    }
    return true;
  }
};

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
void Events::postTouchedEvent()
{
  postEvent(Event { .event = EVENT_SCREEN_TOUCHED });
}

void Events::postHeartbeatEvent() { postEvent({ .event = EVENT_HEARTBEAT }); }
void Events::postAirTemperatureEvent(float x)
{
  static ReduceEventsFrequency< float > r;
  if (!r.skipEvent(x)) {
    postEvent(
        Event { .event = EVENT_SENSOR_TEMPERATURE, .air_temperature = x });
  }
}
void Events::postAirHumidityEvent(float x)
{
  static ReduceEventsFrequency< float > r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_HUMIDITY, .air_humidity = x });
  }
}
void Events::postIAQEvent(float x)
{
  static ReduceEventsFrequency< float > r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_IAQ, .air_iaq = x });
  }
}
void Events::postAirCO2Event(float x)
{
  static ReduceEventsFrequency< float > r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_CO2, .air_co2 = x });
  }
}
void Events::postAirVOCEvent(float x)
{
  static ReduceEventsFrequency< float > r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_VOC, .air_voc = x });
  }
}
void Events::postAirPressureEvent(float x)
{
  static ReduceEventsFrequency< float > r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_PRESSURE, .air_pressure = x });
  }
}

#if ENV_EXT_SENSOR == 1
void Events::postExtTemperatureEvent(float x)
{
  static ReduceEventsFrequency< float > r;
  if (!r.skipEvent(x)) {
    postEvent(Event {
        .event = EVENT_SENSOR_EXT_TEMPERATURE, .air_temperature = x });
  }
}

void Events::postExtHumidityEvent(float x)
{
  static ReduceEventsFrequency< float > r;
  if (!r.skipEvent(x)) {
    postEvent(
        Event { .event = EVENT_SENSOR_EXT_HUMIDITY, .air_humidity = x });
  }
}
#endif

void Events::postOtaStarted() {
  postEvent(Event{ .event = EVENT_OTA_STARTED});
}
void Events::postOtaDoneOk() {
  postEvent(Event{ .event = EVENT_OTA_DONE_OK});
}
void Events::postOtaDoneFail() {
  postEvent(Event{ .event = EVENT_OTA_DONE_FAIL});
}

void Events::postEvent(Event&& theEvent)
{
  if (pdPASS != xQueueSendToBack(event_queue, &theEvent, 10)) {
    ESP_LOGE(TAG, "Failed to post %d event", theEvent.event);
  }
}

MqttEventInfo Events::waitNextEvent()
{
  Event event;
  xQueueReceive(event_queue, &event, portMAX_DELAY);

  MqttEventInfo mqttEvent;
  constexpr size_t DATA_BUSIZE = 8;
  char data[DATA_BUSIZE];
  memset(data, 0, DATA_BUSIZE);
  switch (event.event) {
  case EVENT_HEARTBEAT:
    mqttEvent.name = "heartbeat";
    strcpy(data, "on");
    break;
  case EVENT_SENSOR_TEMPERATURE:
    mqttEvent.name = "air_temperature";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_temperature);
    break;
  case EVENT_SENSOR_HUMIDITY:
    mqttEvent.name = "air_humidity";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_humidity);
    break;
  case EVENT_SCREEN_TOUCHED:
    mqttEvent.name = "touched";
    break;                      // this event does not associate data
  case EVENT_SENSOR_GAS_STATUS: // BSEC_OUTPUT_RUN_IN_STATUS
    mqttEvent.name = "gas_status";
    snprintf(data, DATA_BUSIZE, "%d", event.gas_status);
    break;
  case EVENT_SENSOR_IAQ:
    mqttEvent.name = "air_iaq";
    snprintf(data, DATA_BUSIZE, "%.2f", event.air_iaq);
    break;
  case EVENT_SENSOR_CO2:
    mqttEvent.name = "air_co2";
    snprintf(data, DATA_BUSIZE, "%.2f", event.air_co2);
    break;
  case EVENT_SENSOR_VOC:
    mqttEvent.name = "air_voc";
    snprintf(data, DATA_BUSIZE, "%.2f", event.air_voc);
    break;
  case EVENT_SENSOR_PRESSURE:
    mqttEvent.name = "air_pressure";
    snprintf(data, DATA_BUSIZE, "%.0f", event.air_pressure);
    break;
#if ENV_EXT_SENSOR == 1
  case EVENT_SENSOR_EXT_TEMPERATURE:
    mqttEvent.name = "ext_temperature";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_temperature);
    break;
  case EVENT_SENSOR_EXT_HUMIDITY:
    mqttEvent.name = "ext_humidity";
    snprintf(data, DATA_BUSIZE, "%4.1f", event.air_humidity);
    break;
#endif
  case EVENT_OTA_STARTED:
    mqttEvent.name = "ota";
    strcpy(data, "start");
    break;
  case EVENT_OTA_DONE_OK:
    mqttEvent.name = "ota";
    strcpy(data, "OK");
    break;
  case EVENT_OTA_DONE_FAIL:
    mqttEvent.name = "ota";
    strcpy(data, "FAIL");
    break;
  default:
    ESP_LOGE(TAG, "Unknown event type %d", event.event);
  }
  mqttEvent.data = std::string(data);
  return mqttEvent;
}
