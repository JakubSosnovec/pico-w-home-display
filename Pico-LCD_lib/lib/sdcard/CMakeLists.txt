aux_source_directory(. DIR_SDCARD_SRCS)

add_library(sdcard ${DIR_SDCARD_SRCS})
target_link_libraries(sdcard PUBLIC config)
target_include_directories(sdcard PUBLIC ${CMAKE_CURRENT_LIST_DIR})
