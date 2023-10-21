aux_source_directory(. DIR_CONFIG_SRCS)

add_library(config ${DIR_CONFIG_SRCS})
target_link_libraries(config PUBLIC pico_stdlib hardware_spi)
target_include_directories(config PUBLIC ${CMAKE_CURRENT_LIST_DIR})
