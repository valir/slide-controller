
if(CONFIG_HAS_INTERNAL_SENSOR OR CONFIG_HAS_EXTERNALE_SENSOR)
idf_component_register(
  SRCS
    sensors.cpp
  INCLUDE_DIRS
    "include"
  REQUIRES
    wc-bme280
    wc-bme680
  )
endif()

