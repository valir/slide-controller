# if (CONFIG_USE_SENSOR_BME680)
idf_component_register(
  SRCS
    bme680Sensor.cpp
  INCLUDE_DIRS
    include
  REQUIRES
    Bosch-BSEC2-Library
    events
    wc-sensors
  )

# endif()

