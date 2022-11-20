
idf_component_register(
  SRCS
    wall-controller.cpp
    rs485.cpp
    display.cpp
    touch.cpp
    backlight.cpp
    wifi.cpp
    # modbus.cpp
    statusbar.cpp
    mainpanel.cpp
    gui/lightbulb.c
    gui/lightbulb_outline.c
  INCLUDE_DIRS "."
  )

