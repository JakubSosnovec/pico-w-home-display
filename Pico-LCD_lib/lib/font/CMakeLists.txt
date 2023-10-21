aux_source_directory(. DIR_font_SRCS)

add_library(font ${DIR_font_SRCS})
target_link_libraries(font PUBLIC)
target_include_directories(font PUBLIC ${CMAKE_CURRENT_LIST_DIR})
