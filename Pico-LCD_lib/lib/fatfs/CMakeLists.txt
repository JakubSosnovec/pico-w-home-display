aux_source_directory(. DIR_FATFS_SRCS)

add_library(fatfs ${DIR_FATFS_SRCS})
target_link_libraries(fatfs PUBLIC config hardware_spi lcd font sdcard)
target_include_directories(fatfs PUBLIC ${CMAKE_CURRENT_LIST_DIR})
