
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <freertos/queue.h>
#include <string.h>
#include <string>
#include "events.h"

const char *TAG = "EVENTS";

Events events;

Events::Events() {
  event_queue = xQueueCreate(10, sizeof(Event));
  if (event_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create the queue");
  }
}

Events::~Events(){
  vQueueDelete(event_queue);
}

void Events::postDhtEvent(float temperature, float humidity) {
  Event eventTemperature = {
    .event = EVENT_SENSOR_TEMPERATURE,
    .temperature = temperature
  };
  Event eventHumidity = {
    .event = EVENT_SENSOR_HUMIDITY,
    .relative_humidity = humidity
  };
  if (pdPASS != xQueueSendToBack(event_queue, &eventTemperature, 10) ){
    ESP_LOGE(TAG, "Failed to post Temperature event");
  }
  if (pdPASS != xQueueSendToBack(event_queue, &eventHumidity, 10)){
    ESP_LOGE(TAG, "Failed to post Humidity event");
  }
}

void Events::postMq135Event(float air_quality) {
  Event eventAirQuality = {
    .event = EVENT_SENSOR_AIR_QUALITY,
    .air_quality = air_quality
  };
  if (pdPASS != xQueueSendToBack(event_queue, &eventAirQuality, 10)){
    ESP_LOGE(TAG, "Failed to post Humidity event");
  }
}

MqttEventInfo Events::waitNextEvent() {
  Event event;
  xQueueReceive(event_queue, &event, portMAX_DELAY);

  MqttEventInfo mqttEvent;
#define DATA_BUSIZE 8
  char data[DATA_BUSIZE];
  memset(data, 0, DATA_BUSIZE);
  switch (event.event) {
    case EVENT_SENSOR_TEMPERATURE:
      mqttEvent.name = "air_temperature";
      snprintf(data, DATA_BUSIZE, "%4.1f", event.temperature);
      break;
    case EVENT_SENSOR_HUMIDITY:
      mqttEvent.name = "air_humidity";
      snprintf(data, DATA_BUSIZE, "%4.1f", event.relative_humidity);
      break;
    case EVENT_SENSOR_AIR_QUALITY:
      mqttEvent.name = "air_quality";
      snprintf(data, DATA_BUSIZE, "%4.1f", event.air_quality);
      break;
    default:
      ESP_LOGE(TAG, "Unknown event type %d", event.event);
  }
  mqttEvent.data = std::string(data);
  return mqttEvent;
}
