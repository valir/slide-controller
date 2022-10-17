#ifndef SENSORS_H_INCLUDED
#define SENSORS_H_INCLUDED

enum  SensorsStatus{
  SENSOR_STATUS_STARTING_UP,
  SENSOR_STATUS_STABILIZING,
  SENSOR_STATUS_RUNNING
};

struct Sensors_Info {
  SensorsStatus status = SENSOR_STATUS_STARTING_UP;
  float temperature =0.;
  float relative_humidity =0.;
  float pressure =0.;
  float iaq = 0.;
  float cal_iaq = 0.;
  float cal_temperature = 0.;
  float cal_rel_humidity = 0.;
  float cal_pressure = 0.;
#ifdef ENV_EXT_SENSOR
  float ext_temperature = 0.;
  float ext_humidity = 0.;
  float cal_ext_temperature = 0.;
  float cal_ext_humidity = 0.;
  void set_ext_temperature(float t) { ext_temperature = t + cal_ext_temperature; }
  void set_ext_humidity(float h) { ext_humidity = h + cal_ext_humidity; }
#endif
};

extern Sensors_Info sensors_info;
#endif // SENSORS_H_INCLUDED
