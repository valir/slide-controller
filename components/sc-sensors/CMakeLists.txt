
if(CONFIG_HAS_INTERNAL_SENSOR OR CONFIG_HAS_EXTERNALE_SENSOR)
  set(SENSORS_SRCS sensors.cpp)
endif()

idf_component_register(
  SRCS
    ${SENSORS_SRCS}
  INCLUDE_DIRS
    "include"
  REQUIRES
    sc-bme280
    sc-bme680
    esp32DHT
  )

