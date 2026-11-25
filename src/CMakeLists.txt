# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

set(APP_MODE "modbus" CACHE STRING "Firmware application to build: modbus or pwm")

if(APP_MODE STREQUAL "modbus")
  set(APP_SOURCE "modbus/main.cpp")
elseif(APP_MODE STREQUAL "pwm")
  set(APP_SOURCE "pwm/main.cpp")
else()
  message(FATAL_ERROR "APP_MODE must be either modbus or pwm")
endif()

idf_component_register(SRCS "${APP_SOURCE}")
