idf_component_register(
  SRCS
    "wall-controller.cpp"
    sensors.cpp
    rs485.cpp
    display.cpp
    touch.cpp
    wifi.cpp
    # modbus.cpp
    mqtt.cpp
    events.cpp
    statusbar.cpp
    mainpanel.cpp
    ota.cpp
    bme680-sensor.cpp
    bme280-sensor.cpp
  INCLUDE_DIRS ".")

if (DEFINED ENV{WC_WIFI_SSID} AND DEFINED ENV{WC_HOSTNAME})
  message(STATUS "Building for host ${WC_HOSTNAME}")
  target_compile_options(${COMPONENT_TARGET} PUBLIC
    -DENV_HOSTNAME="$ENV{WC_HOSTNAME}"
    -DENV_WIFI_SSID="$ENV{WC_WIFI_SSID}"
    -DENV_WIFI_PASS="$ENV{WC_WIFI_PASS}"
    -DENV_OTA_IP="$ENV{WC_OTA_IP}"
    -DENV_SENSOR_TYPE=$ENV{WC_SENSOR_TYPE}
    )
else()
  message(STATUS "Please define required env variables")
endif()

