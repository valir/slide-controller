#ifndef SENSORS_H_INCLUDED
#define SENSORS_H_INCLUDED

enum SensorsStatus {
  SENSOR_STATUS_STARTING_UP,
  SENSOR_STATUS_STABILIZING,
  SENSOR_STATUS_RUNNING
};

class Sensors_Info {
  SensorsStatus status = SENSOR_STATUS_STARTING_UP;
  float temperature = 0.;
  float relative_humidity = 0.;
  float pressure = 0.;
  float iaq = 0.;
  float cal_iaq = 0.;
  float cal_temperature = 0.;
  float cal_rel_humidity = 0.;
  float cal_pressure = 0.;
#ifdef CONFIG_HAS_EXTERNAL_SENSOR
  float ext_temperature = 0.;
  float ext_humidity = 0.;
  float cal_ext_temperature = 0.;
  float cal_ext_humidity = 0.;

  public:
  void set_ext_temperature(float t);
  void set_ext_humidity(float h);
#endif
  protected:
  friend class bme280Sensor;
  friend class bme680Sensor;
  void set_status(SensorsStatus s) { status = s; }
  public:
  bool is_running() const { return status == SENSOR_STATUS_RUNNING; }
  void set_temperature(float t);
  void set_rel_humidity(float h);
  void set_pressure(float p);
  void set_cal_values(float t, float h, float i)
  {
    cal_temperature = t;
    cal_rel_humidity = h;
    cal_iaq = i;
  }
};

extern Sensors_Info sensors_info;
#endif // SENSORS_H_INCLUDED
