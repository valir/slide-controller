#ifndef SENSORS_H_INCLUDED
#define SENSORS_H_INCLUDED

enum  SensorsStatus{
  SENSOR_STATUS_STARTING_UP,
  SENSOR_STATUS_STABILIZING,
  SENSOR_STATUS_RUNNING
};

struct Sensors_Info {
  SensorsStatus status = SENSOR_STATUS_STARTING_UP;
  float temperature =0.0;
  float relative_humidity =0.0;
  float iaq = 0.;
  float cal_iaq = 0.;
  float cal_temperature = 0.;
  float cal_rel_humidity = 0.;
};

extern Sensors_Info sensors_info;
#endif // SENSORS_H_INCLUDED
