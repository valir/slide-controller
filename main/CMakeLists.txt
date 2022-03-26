idf_component_register(
  SRCS
    "wall-controller.cpp"
    dht.cpp
    mq135.cpp
    rs485.cpp
    display.cpp
    touch.cpp
    wifi.cpp
    # modbus.cpp
    mqtt.cpp
    events.cpp
  INCLUDE_DIRS ".")
