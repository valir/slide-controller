#ifndef SENSORS_H_INCLUDED
#define SENSORS_H_INCLUDED

enum DHT_Status {
  DHT_STATUS_OK,
  DHT_STATUS_INITIALIZING,
  DHT_STATUS_BAD_CHECKSUM,
  DHT_STATUS_INVALID
};

struct DHT_Info {
};

extern DHT_Info dht_info;

enum MQ135_Status {
  MQ135_STATUS_OK,
  MQ135_STATUS_INITIALIZING,
  MQ135_STATUS_UNCORRECTED,
  MQ135_STATUS_INVALID
};

struct Sensors_Info {
  float temperature =0.0;
  float relative_humidity =0.0;
  uint64_t temp_polling_interval = pdMS_TO_TICKS(30000);
  DHT_Status temp_status = DHT_STATUS_INITIALIZING;
  float ppm = 0.;
  uint64_t voc_polling_interval = pdMS_TO_TICKS(30000);
  MQ135_Status voc_status = MQ135_STATUS_INITIALIZING;
  float cal_ppm = 0.;
  float cal_temperature = 0.;
  float cal_rel_humidity = 0.;
};

extern Sensors_Info sensors_info;
#endif // SENSORS_H_INCLUDED
