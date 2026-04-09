# 🚧 Pothole Severity Classifier
### Modified OpenCV `imgproc` Module — Road Health Analysis System

---

## What This Does

Processes dashcam video (or a live camera / single image) and:

1. **Detects** pothole regions using adaptive thresholding + morphological cleanup
2. **Classifies** each pothole as `MINOR / MODERATE / SEVERE` using area + shadow-depth scoring
3. **Draws** annotated bounding boxes with severity labels on every frame
4. **Renders** a live road-health percentage bar
5. **Accumulates** a damage heatmap across the full video
6. **Exports** a CSV of grid-cell damage scores for GIS / Excel analysis

---

## Project Structure

```
pothole_detector/
├── modules/
│   └── imgproc/
│       ├── include/
│       │   └── pothole_severity.hpp   ← New OpenCV header (your modification)
│       └── src/
│           └── pothole_severity.cpp   ← New OpenCV source  (your modification)
├── main.cpp                           ← Pipeline entry point
├── heatmap.cpp                        ← Grid-based damage accumulator
└── CMakeLists.txt                     ← Build config
```

---

## How to Build (Windows)

### Prerequisites

**1. Install Visual Studio 2019 or 2022**
Download from https://visualstudio.microsoft.com/
During install, select **"Desktop development with C++"** workload.

**2. Install CMake**
Download from https://cmake.org/download/ — pick the Windows `.msi` installer.
✅ Tick **"Add CMake to system PATH"** during install.

**3. Install OpenCV**
- Go to https://opencv.org/releases/ and download the latest **Windows** `.exe`
- Run it — it self-extracts to `C:\opencv` by default
- That's it, no compilation needed (pre-built binaries included)

---

### Build (One-Click)

```
Double-click  build_windows.bat
```

That's literally it. The script will:
1. Find your OpenCV at `C:\opencv\build`
2. Auto-detect Visual Studio version
3. Run CMake + MSBuild in Release mode
4. Copy all required OpenCV `.dll` files next to the `.exe`

**If your OpenCV is NOT at `C:\opencv`**, open `build_windows.bat` in Notepad and change this line:
```bat
set OpenCV_DIR=C:\opencv\build
```

---

### Build (Manual — if you prefer)

Open **Developer Command Prompt for VS** (search in Start menu), then:

```bat
cd pothole_detector
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DOpenCV_DIR="C:\opencv\build"
cmake --build . --config Release
```

Exe will be at: `build\Release\pothole_detector.exe`

---

## How to Run

Open **Command Prompt** or **PowerShell** in the project folder:

```bat
REM On a dashcam video
build\Release\pothole_detector.exe --video road.mp4

REM On your laptop webcam
build\Release\pothole_detector.exe --camera 0

REM On a single image
build\Release\pothole_detector.exe --image road.jpg

REM Save annotated output video
build\Release\pothole_detector.exe --video road.mp4 --save output_annotated.mp4

REM Tune sensitivity (default minArea=300, lower = detect smaller potholes)
build\Release\pothole_detector.exe --video road.mp4 --minarea 150
```

> **Tip:** If you get a `VCRUNTIME140.dll not found` error, install the
> [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).

---

## Keyboard Controls (during video playback)

| Key | Action |
|-----|--------|
| `H` | Toggle heatmap overlay |
| `S` | Save damage heatmap as CSV |
| `R` | Reset heatmap accumulator |
| `Q` / `ESC` | Quit |

---

## The OpenCV Modification Explained

Standard OpenCV's `connectedComponentsWithStats()` gives you:
- Blob count
- Bounding boxes
- Area

**This project extends it with:**

| Addition | File | What it does |
|---|---|---|
| `detectPotholeRegions()` | `pothole_severity.cpp` | Full pipeline: CLAHE → adaptive threshold → morphology → classify |
| `classifySeverity()` | `pothole_severity.cpp` | Scores potholes using `area × darkness_weight` formula |
| `drawPotholeOverlay()` | `pothole_severity.cpp` | Renders severity labels + road health bar |
| `PotholeRegion` struct | `pothole_severity.hpp` | Extended blob metadata (severity, darkness, centroid) |
| `PotholeStats` struct | `pothole_severity.hpp` | Aggregate road health score |
| `RoadHeatmap` class | `heatmap.cpp` | Grid-based accumulator + CSV export |

---

## How the Severity Scoring Works

```
composite_score = area_px × (1.0 + darkness × 0.6)

NONE:     score < 600   OR  darkness < 0.35   (likely road marking / noise)
MINOR:    score < 2500                         (surface crack)
MODERATE: score < 8000                         (medium pothole)
SEVERE:   score ≥ 8000                         (large / deep pothole)
```

`darkness` is the mean pixel darkness in the bounding-box ROI — a proxy for depth (deeper potholes cast darker shadows).

---

## Output Files

After running, the program auto-saves:
- `road_health_final.csv` — Grid damage scores (open in Excel / QGIS)

CSV format:
```
row,col,cell_x_center,cell_y_center,damage_score
0,0,32.0,24.0,0.00
0,1,96.0,24.0,3.50
...
```

---