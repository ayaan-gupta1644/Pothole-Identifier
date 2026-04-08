cmake_minimum_required(VERSION 3.10)
project(pothole_detector)

set(CMAKE_CXX_STANDARD 17)

# Point CMake to your OpenCV installation
set(OpenCV_DIR "C:/opencv/build")

find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(pothole_detector
    main.cpp
    heatmap.cpp
    pothole_severity.cpp
)

target_link_libraries(pothole_detector ${OpenCV_LIBS})