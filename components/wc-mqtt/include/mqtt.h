#ifndef _MQTT_H_
#define _MQTT_H_

#include <events.h>

#include <string>

struct MqttEventInfo {
  const char* name;
  std::string data;
};

#endif /* ifndef _MQTT_H_ */
