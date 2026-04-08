cmake_minimum_required(VERSION 3.14)
project(pothole_detector VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Find OpenCV ──────────────────────────────────────────────
# Adjust the path below if OpenCV is installed in a custom location
# e.g. set(OpenCV_DIR "/usr/local/lib/cmake/opencv4")
find_package(OpenCV REQUIRED COMPONENTS
    core imgproc highgui videoio objdetect)

if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found. Install it with:\n"
            "  sudo apt install libopencv-dev   # Ubuntu/Debian\n"
            "  brew install opencv              # macOS\n")
endif()

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include: ${OpenCV_INCLUDE_DIRS}")

# ── Include dirs ─────────────────────────────────────────────
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}                         # for heatmap.cpp include
    ${CMAKE_SOURCE_DIR}/modules/imgproc/include # for pothole_severity.hpp
)

# ── Sources ──────────────────────────────────────────────────
set(SOURCES
    main.cpp
    modules/imgproc/src/pothole_severity.cpp
    # heatmap.cpp is header-only style, included directly in main.cpp
)

# ── Executable ───────────────────────────────────────────────
add_executable(pothole_detector ${SOURCES})

target_link_libraries(pothole_detector PRIVATE ${OpenCV_LIBS})

# ── Compiler warnings ────────────────────────────────────────
if(MSVC)
    target_compile_options(pothole_detector PRIVATE /W4)
else()
    target_compile_options(pothole_detector PRIVATE -Wall -Wextra -O2)
endif()

# ── Install rule (optional) ──────────────────────────────────
install(TARGETS pothole_detector DESTINATION bin)

# ── Print usage after build ──────────────────────────────────
add_custom_command(TARGET pothole_detector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "✅  Build complete!"
    COMMAND ${CMAKE_COMMAND} -E echo "   Run on a video : ./pothole_detector --video road.mp4"
    COMMAND ${CMAKE_COMMAND} -E echo "   Run on camera  : ./pothole_detector --camera 0"
    COMMAND ${CMAKE_COMMAND} -E echo "   Run on image   : ./pothole_detector --image road.jpg"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)
