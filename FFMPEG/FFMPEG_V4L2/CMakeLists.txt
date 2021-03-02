# cmake needs this line 
cmake_minimum_required(VERSION 2.8) 

# Enable C++11 
set(CMAKE_CXX_STANDARD 11) 
set(CMAKE_CXX_STANDARD_REQUIRED TRUE) 

# Define project name 
project(camera_sdl) 

# Find OpenCV, you may need to set OpenCV_DIR variable 
# to the absolute path to the directory containing OpenCVConfig.cmake file 
# via the command line or GUI 

#find_package(OpenCV 4.1.0 REQUIRED COMPONENTS core highgui calib3d) 
#include_directories( ${OpenCV_INCLUDE_DIRS} ) 
include_directories("/usr/local/include")
link_directories("/usr/local/lib")

# Declare the executable target built from your sources 
add_executable(camera_sdl FrameQueue.cpp PacketQueue.cpp main.cpp Media.cpp Video.cpp VideoDisplay.cpp) 

# Link your application with OpenCV libraries 
#target_link_libraries(camera_sdl ${OpenCV_LIBS})

target_link_libraries(camera_sdl libavcodec.so libavformat.so libswscale.so libavutil.so libavdevice.so libswresample.so 
libavdevice.so libSDL2.so  -lpthread)


