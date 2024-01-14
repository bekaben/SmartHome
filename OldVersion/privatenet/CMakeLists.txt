cmake_minimum_required(VERSION 3.20.0)

if("${BOARD}" STREQUAL "nrf7002dk_nrf5340_cpunet")
  message(STATUS "${BOARD} compile as remote in this application")
else()
  message(FATAL_ERROR "${BOARD} is not supported for this application")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(privatenet)

target_sources(app PRIVATE src/main.c)
