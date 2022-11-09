
// dummy include file for controllers without sensors
#pragma once

class noSensor {
  public:
  void init() {}
  void sensors_timer() {
  sensors_info.status = SENSOR_STATUS_RUNNING;
  }
};
