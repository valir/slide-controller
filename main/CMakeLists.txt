
idf_component_register(
  SRCS
    slide-controller.cpp
    # rs485.cpp
    display.cpp
    touch.cpp
    wifi.cpp
    # modbus.cpp
    statusbar.cpp
    mainpanel.cpp
    gui/lightbulb.c
    gui/lightbulb_outline.c
  INCLUDE_DIRS "."
  )

idf_build_get_property(SC_VERSION PROJECT_VER)
message(STATUS "Building version: ${SC_VERSION}")
idf_build_set_property(COMPILE_OPTIONS "-DSC_VERSION=${SC_VERSION}" APPEND)

