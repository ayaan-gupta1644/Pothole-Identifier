cmake_minimum_required(VERSION 3.14)
project(pothole_detector VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ═══════════════════════════════════════════════════════════════
#  WINDOWS — point CMake at your OpenCV install
#
#  Default path assumes you extracted the official OpenCV 4.x
#  Windows release to C:\opencv.
#
#  If you installed somewhere else, either:
#    • Edit the path below, OR
#    • Pass it on the cmake command line:
#        cmake .. -DOpenCV_DIR="D:\libs\opencv\build"
# ═══════════════════════════════════════════════════════════════
if(WIN32)
    # Try the environment variable first (set by build_windows.bat)
    if(DEFINED ENV{OpenCV_DIR})
        set(OpenCV_DIR "$ENV{OpenCV_DIR}")
    elseif(NOT DEFINED OpenCV_DIR)
        # Fallback default — change this if your path is different
        set(OpenCV_DIR "C:/opencv/build")
    endif()
    message(STATUS "Windows build — OpenCV_DIR = ${OpenCV_DIR}")
endif()

# ── Find OpenCV ──────────────────────────────────────────────
find_package(OpenCV REQUIRED COMPONENTS
    core imgproc highgui videoio objdetect)

if(NOT OpenCV_FOUND)
    if(WIN32)
        message(FATAL_ERROR
            "OpenCV not found!\n"
            "  1. Download from https://opencv.org/releases/\n"
            "  2. Run the .exe installer → extracts to C:\\opencv\n"
            "  3. Re-run cmake with: -DOpenCV_DIR=\"C:/opencv/build\"\n")
    else()
        message(FATAL_ERROR "OpenCV not found.\n"
            "  Ubuntu: sudo apt install libopencv-dev\n"
            "  macOS : brew install opencv\n")
    endif()
endif()

message(STATUS "OpenCV ${OpenCV_VERSION} found at ${OpenCV_INCLUDE_DIRS}")

# ── MSVC-specific flags ───────────────────────────────────────
if(MSVC)
    # Suppress common OpenCV/Windows warnings that clutter build output
    add_compile_options(
        /W3          # warning level 3 (not /W4 — OpenCV headers are noisy)
        /wd4819      # charset warning
        /wd4996      # deprecated CRT functions
        /MP          # parallel compilation
        /O2          # optimise
    )
    # Use the multi-threaded DLL runtime (matches OpenCV pre-builts)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

    # Needed on Windows to avoid min/max macro collisions from <windows.h>
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES -DWIN32_LEAN_AND_MEAN)
endif()

# ── Include dirs ─────────────────────────────────────────────
include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}                           # for heatmap.cpp
    ${CMAKE_SOURCE_DIR}/modules/imgproc/include   # for pothole_severity.hpp
)

# ── Sources ──────────────────────────────────────────────────
set(SOURCES
    main.cpp
    modules/imgproc/src/pothole_severity.cpp
    # heatmap.cpp is header-only style, #included directly by main.cpp
)

# ── Executable ───────────────────────────────────────────────
add_executable(pothole_detector ${SOURCES})

target_link_libraries(pothole_detector PRIVATE ${OpenCV_LIBS})

# ── Windows: copy OpenCV DLLs next to the .exe after build ───
#
#  Without this step the .exe crashes immediately because Windows
#  can't find opencv_world4xx.dll (or the individual module DLLs).
#  CMake's fixup_bundle / file(GET_RUNTIME_DEPENDENCIES) are
#  unreliable for OpenCV, so we use a simple glob copy instead.
# ─────────────────────────────────────────────────────────────
if(WIN32)
    # Determine the bin directory that contains the DLLs.
    # Official OpenCV releases use:  <build>/x64/vc16/bin/  (VS 2019)
    #                             or <build>/x64/vc17/bin/  (VS 2022)
    # We try both and pick whichever exists.
    set(OPENCV_BIN_CANDIDATES
        "${OpenCV_DIR}/x64/vc17/bin"
        "${OpenCV_DIR}/x64/vc16/bin"
        "${OpenCV_DIR}/x64/vc15/bin"
        "${OpenCV_DIR}/bin"
    )

    set(OPENCV_BIN_DIR "")
    foreach(candidate ${OPENCV_BIN_CANDIDATES})
        if(EXISTS "${candidate}")
            set(OPENCV_BIN_DIR "${candidate}")
            break()
        endif()
    endforeach()

    if(OPENCV_BIN_DIR STREQUAL "")
        message(WARNING
            "Could not find OpenCV DLL directory automatically.\n"
            "You may need to copy OpenCV DLLs manually next to pothole_detector.exe\n"
            "OR add the OpenCV bin folder to your PATH.")
    else()
        message(STATUS "OpenCV DLL source: ${OPENCV_BIN_DIR}")

        # Copy every .dll from that directory to the output folder
        add_custom_command(TARGET pothole_detector POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Copying OpenCV DLLs from ${OPENCV_BIN_DIR} ..."
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${OPENCV_BIN_DIR}"
                "$<TARGET_FILE_DIR:pothole_detector>"
            COMMENT "Deploying OpenCV DLLs"
        )
    endif()
endif()

# ── Install rule ─────────────────────────────────────────────
install(TARGETS pothole_detector DESTINATION bin)

# ── Post-build usage message ─────────────────────────────────
add_custom_command(TARGET pothole_detector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Build complete!"
    COMMAND ${CMAKE_COMMAND} -E echo "  pothole_detector.exe --video road.mp4"
    COMMAND ${CMAKE_COMMAND} -E echo "  pothole_detector.exe --camera 0"
    COMMAND ${CMAKE_COMMAND} -E echo "  pothole_detector.exe --image road.jpg"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)
