if(CONFIG_USE_SENSOR_BME680)
  set(BME680_SRCS bme680Sensor.cpp)
endif()

idf_component_register(
  SRCS
    ${BME680_SRCS}
  INCLUDE_DIRS
    include
  REQUIRES
    Bosch-BSEC2-Library
    sc-events
    sc-sensors
  )
