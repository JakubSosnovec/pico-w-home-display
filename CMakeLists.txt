cmake_minimum_required(VERSION 3.18)
set(PICO_BOARD pico_w)
set(PICO_SDK_FETCH_FROM_GIT on)
set(CFLAGS "-I${PROJECT_SOURCE_DIR} -DMBEDTLS_CONFIG_FILE='<mbedtls_config.h>'") # Total hack, but taken from official TLS docs
include(pico_sdk_import.cmake)

project(weather_display C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(Pico-LCD_lib)

add_executable(weather_display
        main.c
        )
target_compile_definitions(weather_display PRIVATE
        WIFI_SSID=\"${WIFI_SSID}\"
        WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
        PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=3000
        ALTCP_MBEDTLS_AUTHMODE=MBEDTLS_SSL_VERIFY_REQUIRED
        )
target_include_directories(weather_display PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        )
target_link_libraries(weather_display PRIVATE
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_mbedtls
        pico_mbedtls
        pico_stdlib
        pico_stdio_usb
        config
        lcd
        font
        )

pico_enable_stdio_usb(weather_display 1)
pico_enable_stdio_uart(weather_display 0)

# Configure binary output
#
#
#   Request additional binary formats (.uf2, .hex, etc.) to be built.
#
#   Defined in Pico SDK macros.
#
#   https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common.cmake
#
pico_add_extra_outputs(weather_display)

# Ignore warnings from lwip code
set_source_files_properties(
        ${PICO_LWIP_PATH}/src/apps/altcp_tls/altcp_tls_mbedtls.c
        PROPERTIES
        COMPILE_OPTIONS "-Wno-unused-result"
        )
