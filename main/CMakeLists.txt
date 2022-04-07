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
  INCLUDE_DIRS ".")

target_compile_options(${COMPONENT_TARGET} PUBLIC
  -DENV_HOSTNAME="$ENV{WC_HOSTNAME}"
  -DENV_WIFI_SSID="$ENV{WC_WIFI_SSID}"
  -DENV_WIFI_PASS="$ENV{WC_WIFI_PASS}")


