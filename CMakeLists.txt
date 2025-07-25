cmake_minimum_required(VERSION 3.10)
project(RTSPDecodeExample)

set(CMAKE_CXX_STANDARD 14)

set(ffmpeg_libs_DIR /usr/lib/aarch64-linux-gnu)                         ## ffmpeg
set(ffmpeg_headers_DIR /usr/include/aarch64-linux-gnu) 


set(nvjmi_libs_DIR /usr/local/lib)               ## nvjmi
set(nvjmi_headers_DIR /usr/local/include)
add_library(nvjmi SHARED IMPORTED )
set_target_properties(nvjmi PROPERTIES IMPORTED_LOCATION ${nvjmi_libs_DIR}/libnvjmi.so )

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})


add_library( avcodec SHARED IMPORTED )
 
set_target_properties( avcodec PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavcodec.so )

 
add_library( swresample SHARED IMPORTED )
set_target_properties( swresample PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libswresample.so )
 
add_library( swscale SHARED IMPORTED )
set_target_properties( swscale PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libswscale.so )
 
add_library( avformat SHARED IMPORTED )
set_target_properties( avformat PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavformat.so )
 
add_library( avutil SHARED IMPORTED )
set_target_properties( avutil PROPERTIES IMPORTED_LOCATION ${ffmpeg_libs_DIR}/libavutil.so ) 

add_library(RtspClientLib SHARED  video_capture.cpp)

add_executable(RTSPDecode
    video_capture.h
    video_capture.cpp
    main.cpp
)

target_include_directories(RTSPDecode PRIVATE ${nvjmi_headers_DIR} ${ffmpeg_headers_DIR} )

target_link_libraries(RTSPDecode      
    ${OpenCV_LIBS}
    -pthread                                       
    avformat avcodec nvjmi   avutil  swscale swresample
)

target_link_libraries(RtspClientLib     
    ${OpenCV_LIBS}
    -pthread                                       
    avformat avcodec nvjmi   avutil  swscale swresample 
)
