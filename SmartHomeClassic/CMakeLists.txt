cmake_minimum_required(VERSION 3.20.0)
set(BOARD "nrf7002dk_nrf5340_cpuapp")
set(BOARD_REMOTE "nrf7002dk_nrf5340_cpunet")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(smarthome)

target_sources(app PRIVATE src/main.c)


include(ExternalProject)

ExternalProject_Add(
  privatenet
  SOURCE_DIR ${APPLICATION_SOURCE_DIR}/privatenet
  INSTALL_COMMAND ""      # This particular build system has no install command
  CMAKE_CACHE_ARGS -DBOARD:STRING=${BOARD_REMOTE}
  BUILD_BYPRODUCTS "${REMOTE_ZEPHYR_DIR}/${KERNEL_BIN_NAME}"
  # NB: Do we need to pass on more CMake variables?
  BUILD_ALWAYS True
)
