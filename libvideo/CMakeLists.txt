cmake_minimum_required(VERSION 3.0)

project(LibVideo C)

include_directories(./)

add_compile_options(-Wall -fPIC)

add_library(video SHARED video.c)

target_link_libraries(video avcodec avformat avutil swscale)

install(TARGETS video DESTINATION .)