# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

file(STRINGS "version.txt" PROJECT_VER)

if(EXISTS "$ENV{SC_CONFIG_DIR}/sdkconfig.common")
set(SDKCONFIG_DEFAULTS
  "../sdkconfig.common;../sdkconfig.wifi;../sdkconfig.$ENV{SC_HOSTNAME}")
  set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(swipe-controller)
#
# add_dependencies(${COMPONENT_LIB} next_version)

