#pragma once

#include "bme-sensor.h"

class bme280Sensor : public bmeSensor<int8_t, 0, -1> {
  int8_t _res;
  uint32_t _read_delay_until;
  uint32_t _req_delay;

  public:
  void init();
  void sensors_timer();
};
