
idf_component_register(
  SRCS
    mqtt.cpp
    ota.cpp
  INCLUDE_DIRS
    "include"
  PRIV_REQUIRES app_update mqtt wc-buzzer wc-sensors wc-events
  )
