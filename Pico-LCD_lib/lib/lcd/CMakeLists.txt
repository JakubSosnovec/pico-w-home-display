aux_source_directory(. DIR_LCD_SRCS)

add_library(lcd ${DIR_LCD_SRCS})
target_link_libraries(lcd PUBLIC config font fatfs pico_stdlib)
target_include_directories(lcd PUBLIC ${CMAKE_CURRENT_LIST_DIR})
