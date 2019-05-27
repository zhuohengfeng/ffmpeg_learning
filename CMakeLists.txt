cmake_minimum_required(VERSION 3.13)

project(ffmpeg_learning)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "-g -Wall")

set(FFMPEG_PATH "/usr/local/Cellar/ffmpeg/4.1.1")

INCLUDE_DIRECTORIES(${FFMPEG_PATH}/include)

LINK_DIRECTORIES(${FFMPEG_PATH}/lib)

add_executable(ffmpeg_learning
        #main_io.cpp
        main_format.cpp
        #main_codec.cpp
        )

TARGET_LINK_LIBRARIES(ffmpeg_learning
        avcodec avdevice avfilter avformat avresample avutil postproc swresample swscale)