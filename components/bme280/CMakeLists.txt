if (CONFIG_USE_SENSOR_BME280)
  idf_component_register(
    SRCS bme280Sensor.cpp
    INCLUDE_DIRS "include"
    REQUIRES
      Bosch-BME280-driver
      events
      wc-sensors
    )
endif()

