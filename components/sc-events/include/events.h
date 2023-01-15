#ifndef _EVENTS_H_INCLUDED_
#define _EVENTS_H_INCLUDED_

#include <esp_event.h>
#include <lvgl.h>

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
#if ENV_EXT_SENSOR == 1
  EVENT_SENSOR_EXT_TEMPERATURE,
  EVENT_SENSOR_EXT_HUMIDITY,
#endif
  EVENT_OTA_STARTED,
  EVENT_OTA_DONE_OK,
  EVENT_OTA_DONE_FAIL
};

enum WallControllerStatus {
  STATUS_DHT_OK = 0x00,
  STATUS_DHT_FAIL = 0x01,
};

enum TouchGesture {
  TOUCH_GESTURE_OFF =0, // special value meaning no gesture was actually found
  TOUCH_GESTURE_SHORT_PRESS,
  TOUCH_GESTURE_LONG_PRESS,
  TOUCH_GESTURE_SWIPE_LEFT,
  TOUCH_GESTURE_SWIPE_RIGHT,
  TOUCH_GESTURE_SWIPE_UP,
  TOUCH_GESTURE_SWIPE_DOWN,
  TOUCH_GESTURE_MAX
};

struct Event {
  WallControllerEvent event;
  union {
    WallControllerStatus status;
    struct {
      TouchGesture gesture;
      lv_point_t p;
    } touch_info;
    float air_temperature;
    float air_humidity;
    bool gas_status;
    float air_iaq;
    float air_co2;
    float air_voc;      //
    float air_pressure; // hPa
  };
};

struct EventObserver {
  virtual void notice(const Event&) = 0;
  virtual const char* name() =0;
};

class Events {
  public:
  Events();
  virtual ~Events();
  void postHeartbeatEvent();
  void postAirTemperatureEvent(float);
  void postAirHumidityEvent(float);
  void postIAQEvent(float);
  void postAirCO2Event(float);
  void postAirVOCEvent(float);
  void postAirPressureEvent(float);
  void postTouchedEvent(TouchGesture, lv_point_t);
  void postExtTemperatureEvent(float);
  void postExtHumidityEvent(float);
  void postOtaStarted();
  void postOtaDoneOk();
  void postOtaDoneFail();
  void registerObserver(EventObserver*);

  private:
  void postEvent(const Event& theEvent);
};

extern Events events;

#endif /* ifndef _EVENTS_H_INCLUDED_ */
