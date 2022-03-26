#ifndef _EVENTS_H_INCLUDED_
#define _EVENTS_H_INCLUDED_

#include <esp_event.h>
#include <mqtt.h>

enum WallControllerEvent {
  EVENT_STATUS_UPDATE,
  EVENT_SENSOR_TEMPERATURE,
  EVENT_SENSOR_HUMIDITY,
  EVENT_SENSOR_AIR_QUALITY
};

enum WallControllerStatus {
  STATUS_DHT_OK = 0x00,
  STATUS_DHT_FAIL = 0x01,
};

struct Event {
  WallControllerEvent event;
  union {
    WallControllerStatus status;
    float temperature;
    float relative_humidity;
    float air_quality;
  };
};

class Events
{
public:
  Events ();
  virtual ~Events ();
  void postDhtEvent(float temperature, float humidity);
  void postMq135Event(float air_quality);
  MqttEventInfo waitNextEvent();

private:
  QueueHandle_t event_queue;
};

extern Events events;

#endif /* ifndef _EVENTS_H_INCLUDED_ */
