cmake_minimum_required(VERSION 3.18)
set(PICO_BOARD pico_w)
set(PICO_SDK_FETCH_FROM_GIT on)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/proto/nanopb/extra)
find_package(Nanopb REQUIRED)
nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS proto/gtfs-realtime-OVapi.proto)
nanopb_generate_cpp(PROTO_SRCS PROTO_HDRS proto/gtfs-realtime.proto)

# Total hack, but taken from official TLS docs
set(CFLAGS "-I${PROJECT_SOURCE_DIR} -DMBEDTLS_CONFIG_FILE='<mbedtls_config.h>'")
include(pico_sdk_import.cmake)

project(weather_display C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(WIFI_SSID "" CACHE STRING "WIFI SSID")
set(WIFI_PASSWORD "" CACHE STRING "WIFI password")
set(GOLEMIO_API_KEY "" CACHE STRING "Golemio API key")
if(WIFI_SSID STREQUAL "")
    message(FATAL_ERROR "You must set WIFI SSID with WIFI_SSID variable")
endif()
if(WIFI_PASSWORD STREQUAL "")
    message(FATAL_ERROR "You must set WIFI password with WIFI_PASSWORD variable")
endif()
if(GOLEMIO_API_KEY STREQUAL "")
    message(FATAL_ERROR "You must set Golemio API key with GOLEMIO_API_KEY variable")
endif()

add_subdirectory(Pico-LCD_lib)

add_executable(weather_display main.c log/log.c ${PROTO_SRCS})
target_compile_definitions(
  weather_display
  PRIVATE WIFI_SSID=\"${WIFI_SSID}\"
          WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
          GOLEMIO_API_KEY=\"${GOLEMIO_API_KEY}\"
          PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=3000
          ALTCP_MBEDTLS_AUTHMODE=MBEDTLS_SSL_VERIFY_REQUIRED)
target_include_directories(
  weather_display PRIVATE ${CMAKE_CURRENT_LIST_DIR}
                          ${CMAKE_CURRENT_LIST_DIR}/log
                          ${NANOPB_INCLUDE_DIRS})
target_link_libraries(
  weather_display
  PRIVATE pico_cyw43_arch_lwip_threadsafe_background
          pico_lwip_mbedtls
          pico_mbedtls
          pico_stdlib
          pico_stdio_usb
          hardware_rtc
          config
          lcd
          font)

pico_enable_stdio_usb(weather_display 1)
pico_enable_stdio_uart(weather_display 0)

pico_add_extra_outputs(weather_display)

# Ignore warnings from lwip code
set_source_files_properties(
  ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
  PROPERTIES COMPILE_OPTIONS "-Wno-unused-result")
