
#include "events.h"
#include <algorithm>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <list>
#include <string.h>
#include <string>

const char* TAG = "EVENTS";

Events events;
static QueueHandle_t event_queue;

Events::Events()
{
  event_queue = xQueueCreate(10, sizeof(Event));
  if (event_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create the queue");
  }
}

Events::~Events() { vQueueDelete(event_queue); }

template <typename T> struct ReduceEventsFrequency {
  T last_x;
  uint8_t count = 0;
  ReduceEventsFrequency(T zero = 0)
      : last_x(zero)
  {
  }
  bool skipEvent(T x)
  {
    if (last_x != x || count++ > 6) {
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
  static ReduceEventsFrequency<float> r;
  if (!r.skipEvent(x)) {
    postEvent(
        Event { .event = EVENT_SENSOR_TEMPERATURE, .air_temperature = x });
  }
}
void Events::postAirHumidityEvent(float x)
{
  static ReduceEventsFrequency<float> r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_HUMIDITY, .air_humidity = x });
  }
}
void Events::postIAQEvent(float x)
{
  static ReduceEventsFrequency<float> r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_IAQ, .air_iaq = x });
  }
}
void Events::postAirCO2Event(float x)
{
  static ReduceEventsFrequency<float> r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_CO2, .air_co2 = x });
  }
}
void Events::postAirVOCEvent(float x)
{
  static ReduceEventsFrequency<float> r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_VOC, .air_voc = x });
  }
}
void Events::postAirPressureEvent(float x)
{
  static ReduceEventsFrequency<float> r;
  if (!r.skipEvent(x)) {
    postEvent(Event { .event = EVENT_SENSOR_PRESSURE, .air_pressure = x });
  }
}

#if ENV_EXT_SENSOR == 1
void Events::postExtTemperatureEvent(float x)
{
  static ReduceEventsFrequency<float> r;
  if (!r.skipEvent(x)) {
    postEvent(Event {
        .event = EVENT_SENSOR_EXT_TEMPERATURE, .air_temperature = x });
  }
}

void Events::postExtHumidityEvent(float x)
{
  static ReduceEventsFrequency<float> r;
  if (!r.skipEvent(x)) {
    postEvent(
        Event { .event = EVENT_SENSOR_EXT_HUMIDITY, .air_humidity = x });
  }
}
#endif

void Events::postOtaStarted()
{
  postEvent(Event { .event = EVENT_OTA_STARTED });
}
void Events::postOtaDoneOk()
{
  postEvent(Event { .event = EVENT_OTA_DONE_OK });
}
void Events::postOtaDoneFail()
{
  postEvent(Event { .event = EVENT_OTA_DONE_FAIL });
}

void Events::postEvent(Event&& theEvent)
{
  if (pdPASS != xQueueSendToBack(event_queue, &theEvent, 10)) {
    ESP_LOGE(TAG, "Failed to post %d event", theEvent.event);
  }
}

static std::list<EventObserver*> observers;

void Events::registerObserver(EventObserver* observer)
{
  observers.push_back(observer);
}

void eventsTask(void*)
{
  ESP_LOGI(TAG, "Starting events task");

  while (true) {
    Event event;
    if (pdTRUE == xQueueReceive(event_queue, &event, portMAX_DELAY)) {
      std::for_each(
          observers.begin(), observers.end(), [=](EventObserver* observer) {
            if (observer != nullptr) {
              ESP_LOGD(TAG, "Notifying %s", observer->name());
              observer->notice(event);
            } else {
              assert(0);
            }
          });
    }
  }

  vTaskDelete(nullptr);
}
