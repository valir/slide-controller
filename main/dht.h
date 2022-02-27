#ifndef DHT_H_INCLUDED
#define DHT_H_INCLUDED

enum DHT_Status {
  DHT_STATUS_OK,
  DHT_STATUS_BAD_CHECKSUM,
  DHT_STATUS_INVALID
};

struct DHT_Info {
  float temperature =0.0;
  float relative_humidity =0.0;
  uint64_t polling_interval = pdMS_TO_TICKS(30000);
  DHT_Status status = DHT_STATUS_INVALID;
};

extern DHT_Info dht_info;

#endif // DHT_H_INCLUDED
