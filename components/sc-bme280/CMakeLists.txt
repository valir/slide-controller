if(CONFIG_USE_SENSOR_BME280)
  SET(BME280_SRCS
    bme280Sensor.cpp
  )
endif()

idf_component_register(
  SRCS
    ${BME280_SRCS}
    INCLUDE_DIRS "include"
  REQUIRES
    Bosch-BME280-driver
    sc-events
    sc-sensors
  )

