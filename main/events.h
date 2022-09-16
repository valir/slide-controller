#ifndef _EVENTS_H_INCLUDED_
#define _EVENTS_H_INCLUDED_

#include <esp_event.h>
#include <mqtt.h>

enum WallControllerEvent {
  EVENT_STATUS_UPDATE,
  EVENT_HEARTBEAT,
  EVENT_SENSOR_TEMPERATURE,
  EVENT_SENSOR_HUMIDITY,
  EVENT_SCREEN_TOUCHED,
  EVENT_SENSOR_GAS_STATUS, // BSEC_OUTPUT_RUN_IN_STATUS
  EVENT_SENSOR_IAQ,
  EVENT_SENSOR_CO2,
  EVENT_SENSOR_VOC,
  EVENT_SENSOR_PRESSURE,
};

enum WallControllerStatus {
  STATUS_DHT_OK = 0x00,
  STATUS_DHT_FAIL = 0x01,
};

struct Event {
  WallControllerEvent event;
  union {
    WallControllerStatus status;
    float air_temperature;
    float air_humidity;
    bool gas_status;
    float air_iaq;
    float air_co2;
    float air_voc; //
    float air_pressure; // hPa
  };
};

class Events
{
public:
  Events ();
  virtual ~Events ();
  void postHeartbeatEvent();
  void postAirTemperatureEvent(float);
  void postAirHumidityEvent(float);
  void postGasStatusEvent(bool);
  void postIAQEvent(float);
  void postAirCO2Event(float);
  void postAirVOCEvent(float);
  void postAirPressureEvent(float);
  void postTouchedEvent();
  MqttEventInfo waitNextEvent();

private:
  void postEvent(Event&& theEvent);
  QueueHandle_t event_queue;
};

extern Events events;

#endif /* ifndef _EVENTS_H_INCLUDED_ */
