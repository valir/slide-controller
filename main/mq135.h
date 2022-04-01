#ifndef __MQ135_H__
#define __MQ135_H__ value

enum MQ135_Status {
  MQ135_STATUS_OK,
  MQ135_STATUS_UNCORRECTED,
  MQ135_STATUS_INVALID
};

struct MQ135_Info {
  float ppm = 0.;
  uint64_t polling_interval = pdMS_TO_TICKS(30000);
  MQ135_Status status = MQ135_STATUS_INVALID;
  float cal_ppm = 0.;
  float cal_temperature = 0.;
  float cal_rel_humidity = 0.;
};

extern MQ135_Info mq135_info;

#endif /* ifndef __MQ135_H__ */
