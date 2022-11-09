if ( NOT "$ENV{WC_SENSOR_TYPE}" STREQUAL "none")
  SET(SENSORS_SRCS
    sensors.cpp
    bme680Sensor.cpp
    bme280Sensor.cpp)
endif()

idf_component_register(
  SRCS
    wall-controller.cpp
    rs485.cpp
    display.cpp
    touch.cpp
    backlight.cpp
    wifi.cpp
    # modbus.cpp
    mqtt.cpp
    events.cpp
    statusbar.cpp
    mainpanel.cpp
    ota.cpp
    buzzer.cpp
    ${SENSORS_SRCS}
  INCLUDE_DIRS "."
  )

if (DEFINED ENV{WC_WIFI_SSID} AND
    DEFINED ENV{WC_HOSTNAME})
  message(STATUS "Building for host ${WC_HOSTNAME}")
  if ("$ENV{WC_SENSOR_TYPE}" STREQUAL "none")
    target_compile_options(${COMPONENT_TARGET} PUBLIC
      -DENV_HOSTNAME="$ENV{WC_HOSTNAME}"
      -DENV_WIFI_SSID="$ENV{WC_WIFI_SSID}"
      -DENV_WIFI_PASS="$ENV{WC_WIFI_PASS}"
      -DENV_OTA_IP="$ENV{WC_OTA_IP}"
      -DENV_NO_SENSOR
      -DENV_EXT_SENSOR=$ENV{WC_EXT_SENSOR}
      )
  else()
    target_compile_options(${COMPONENT_TARGET} PUBLIC
      -DENV_HOSTNAME="$ENV{WC_HOSTNAME}"
      -DENV_WIFI_SSID="$ENV{WC_WIFI_SSID}"
      -DENV_WIFI_PASS="$ENV{WC_WIFI_PASS}"
      -DENV_OTA_IP="$ENV{WC_OTA_IP}"
      -DENV_SENSOR_TYPE=$ENV{WC_SENSOR_TYPE}
      -DENV_SENSOR_INCLUDE="$ENV{WC_SENSOR_TYPE}.h"
      -DENV_EXT_SENSOR=$ENV{WC_EXT_SENSOR}
      )
  endif()
else()
  message(STATUS "Please define required env variables")
endif()

