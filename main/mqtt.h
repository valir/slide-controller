#ifndef _MQTT_H_
#define _MQTT_H_

#include <string>

struct MqttEventInfo {
  const char *name;
  std::string data;
};

#endif /* ifndef _MQTT_H_ */
